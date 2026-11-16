#pragma once
namespace animation_dirty_reuse {
// Authenticated native parent mutex. Reuse only ownership already held by
// currentThread; failure leaves every field untouched and requires native notify.
bool TryOwned(void* parent,unsigned currentThread);
}
