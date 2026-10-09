#include "../src/TraderLayoutRecognition.h"
#include "../src/TraderTextUnicode.h"
#include "../src/TraderInlinePlacement.h"
#include "../src/TraderMoveTransaction.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <cstdlib>

struct Widget
{
    std::string name, type;
    std::vector<Widget*> children;
    Widget(const std::string& n, const std::string& t) : name(n), type(t) {}
    ~Widget() { for (size_t i = 0; i < children.size(); ++i) delete children[i]; }
    const std::string& getName() const { return name; }
    const std::string& getTypeName() const { return type; }
    size_t getChildCount() { return children.size(); }
    Widget* getChildAt(size_t i) { return children[i]; }
};

void Check(bool ok, const std::string& message)
{
    if (!ok) { std::cerr << "FAIL: " << message << std::endl; std::exit(1); }
}

struct TestMove { int item, from, to; TestMove(int i, int a, int b) : item(i), from(a), to(b) {} };
struct TestMoves
{
    int cells[6]; int failRemove; int rejectTarget;
    TestMoves() : failRemove(-1), rejectTarget(-1)
    { for (int i=0;i<6;++i) cells[i]=-1; cells[0]=0; cells[2]=1; cells[4]=2; }
    bool remove(const TestMove& m)
    { if (m.item==failRemove || cells[m.from]!=m.item) return false; cells[m.from]=-1; return true; }
    bool canPlace(const TestMove& m) { return m.item!=rejectTarget && cells[m.to]==-1; }
    void restore(const TestMove& m) { cells[m.from]=m.item; }
    void addTarget(const TestMove& m) { cells[m.to]=m.item; }
    bool original() const { return cells[0]==0 && cells[2]==1 && cells[4]==2 && cells[1]==-1 && cells[3]==-1 && cells[5]==-1; }
};

int main(int argc, char** argv)
{
    Check(argc == 2, "fixture argument");
    std::ifstream input(argv[1]);
    Check(input.good(), "open fixture");
    std::string line, label;
    int count = 0;
    while (std::getline(input, label))
    {
        if (label.empty()) continue;
        Widget root("fixture", "Widget");
        while (std::getline(input, line) && line != "END")
        {
            std::istringstream row(line);
            std::string type, name;
            row >> type >> name;
            root.children.push_back(new Widget(name, type));
        }
        const bool expected = label == "Kenshi_InventoryTraderWindow.layout";
        Check(TraderLayoutRecognition::IsTrader(&root) == expected, label);
        for (size_t i = 0; i < root.children.size(); ++i)
            root.children[i]->name = "instance_42_" + root.children[i]->name;
        Check(TraderLayoutRecognition::IsTrader(&root) == expected, label + " prefixed");
        ++count;
    }
    Check(count > 5, "enough real layout fixtures");
    Check(!TraderLayoutRecognition::IsTrader<Widget>(0), "null");
    Widget trader("Root", "Window");
    const char* names[] = {"ArrangeButton", "scrollview_backpack_content", "backpack_content", "datapanel"};
    for (size_t i = 0; i < 4; ++i)
    {
        Check(!TraderLayoutRecognition::IsTrader(&trader), "partial creation");
        trader.children.push_back(new Widget(names[i], "Widget"));
    }
    Check(TraderLayoutRecognition::IsTrader(&trader), "ready empty trader");
    Widget outer("outer", "Widget");
    outer.children.push_back(&trader);
    Check(!TraderLayoutRecognition::IsTrader(&outer), "do not merge nested windows");
    outer.children.clear();
    trader.children.push_back(new Widget("OpenBagButton", "Button"));
    Check(!TraderLayoutRecognition::IsTrader(&trader), "equipment exclusion");
    // U+041C U+0415 U+0427 -> U+043C U+0435 U+0447 (Russian sword).
    Check(TraderTextUnicode::NormalizeSearchTextUtf8OrAscii("\xD0\x9C\xD0\x95\xD0\xA7")
        == "\xD0\xBC\xD0\xB5\xD1\x87", "Cyrillic case folding");
    std::vector<TestMove> moves;
    moves.push_back(TestMove(0,0,2)); moves.push_back(TestMove(1,2,0)); moves.push_back(TestMove(2,4,1));
    TestMoves success;
    Check(TraderMoveTransaction::Apply(moves, success), "commit native moves");
    Check(success.cells[0]==1 && success.cells[1]==2 && success.cells[2]==0 && success.cells[4]==-1, "committed cells match targets");
    for (int failure=0;failure<3;++failure)
    {
        TestMoves removal; removal.failRemove=failure;
        Check(!TraderMoveTransaction::Apply(moves,removal) && removal.original(), "rollback failed removal");
        TestMoves placement; placement.rejectTarget=failure;
        Check(!TraderMoveTransaction::Apply(moves,placement) && placement.original(), "rollback rejected target");
    }
    std::cout << "PASS: native packing commit and six rollback cases" << std::endl;
    using namespace TraderInlinePlacement;
    Rect field = Place(960, Rect(20, 12, 250, 34));
    Check(field.left == 278 && field.top == 12 && field.width == 666 && field.height == 34, "field next to money");
    field = Place(280, Rect(20, 12, 250, 34));
    Check(field.width == 0, "insufficient header space");
    for (int width = 500; width <= 1600; width += 10)
    {
        field = Place(width, Rect(12, 6, 210, 30));
        Check(field.left > 222 && field.left + field.width == width - 16
            && field.top == 6 && field.height == 30, "header layout preserves money and grid");
    }
    std::cout << "PASS: inline header placement, narrow header and 111 window widths" << std::endl;
    std::cout << "PASS: " << count << " installed layouts, prefixed names, partial creation, nested windows, equipment exclusion, Cyrillic" << std::endl;
}
