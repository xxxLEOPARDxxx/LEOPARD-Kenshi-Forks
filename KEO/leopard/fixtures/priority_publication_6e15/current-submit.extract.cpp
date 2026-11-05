void hook_pathReqSubmit(void* sectionMgr, void* requestObj, bool highPriority)
{
	planner::PlannerWaterOnSubmit(requestObj ? (float*)((uintptr_t)requestObj + OFF_REQ_WATER_COST_MULT) : NULL);
	game::g_hookOrig.orig_pathReqSubmit(sectionMgr, requestObj, highPriority);

	if (squadBoostTier > 0)
	{
		int gamePri = *(int*)((uintptr_t)requestObj + 44);
		if (squadBoostTier > gamePri)
		{
			*(int*)((uintptr_t)requestObj + 44) = squadBoostTier;
			if (squadBoostFromRepathTier)
			{
				InterlockedIncrement(&p12DiagSubmitBoostsState6);
				PlayerRepathTierNoteTiered();
				// Not counted into playerRequestsInFlight: that counter feeds
				// the 4x A* search-budget boost in hook_findPathFull, a
				// separate lever this change does not touch. Only the queue
				// tier (req+44) changes here.
			}
			else
			{
				InterlockedIncrement(&p12DiagSubmitBoostsOrder);
				InterlockedIncrement(&pathfind::g_pathDiag.playerRequestsInFlight);
			}
		}
	}


	squadBoostTier = 0;
	squadBoostFromRepathTier = false;
}
