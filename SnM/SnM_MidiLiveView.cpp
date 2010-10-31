/******************************************************************************
/ SnM_MidiLiveView.cpp
/
/ Copyright (c) 2009-2010 Tim Payne (SWS), JF BÈdague 
/ http://www.standingwaterstudios.com/reaper
/
/ Permission is hereby granted, free of charge, to any person obtaining a copy
/ of this software and associated documentation files (the "Software"), to deal
/ in the Software without restriction, including without limitation the rights to
/ use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
/ of the Software, and to permit persons to whom the Software is furnished to
/ do so, subject to the following conditions:
/ 
/ The above copyright notice and this permission notice shall be included in all
/ copies or substantial portions of the Software.
/ 
/ THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
/ EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
/ OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
/ NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
/ HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
/ WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
/ FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
/ OTHER DEALINGS IN THE SOFTWARE.
/
******************************************************************************/

//JFB TODO?:
// max nb of tracks & presets to check
// FX chains: + S&M's FX chain view slots
// full release?

#include "stdafx.h"
#include "SnM_Actions.h"
#include "SnM_MidiLiveView.h"
#include "SNM_ChunkParserPatcher.h"
#include "SnM_Chunk.h"

#define SAVEWINDOW_POS_KEY "S&M - Live Configs List Save Window Position"
#define NB_CC_VALUES				128

enum
{
  COMBOID_CONFIG=1000,
  BUTTONID_ENABLE,
  BUTTONID_AUTO_RCV,
  BUTTONID_MUTE_OTHERS,
  COMBOID_INPUT_TRACK
};

// Globals
/*JFB static*/ SnM_MidiLiveWnd* g_pMidiLiveWnd = NULL;
SWSProjConfig<WDL_PtrList_DeleteOnDestroy<WDL_PtrList_DeleteOnDestroy<MidiLiveItem> > > g_liveCCConfigs;
SWSProjConfig<MidiLiveConfig> g_liveConfigs;

int g_configId = 0; // the current *displayed* config id
static SWS_LVColumn g_midiLiveCols[] = { {95,2,"CC value"}, {150,1,"Desc."}, {150,2,"Track"}, {175,2,"Track template"}, {175,2,"FX Chain"}, /*presets {150,2,"FX Presets"},*/ {150,1,"Activate action"}, {150,1,"Deactivate action"}};


bool AddFXSubMenu(HMENU* _menu, MediaTrack* _tr, WDL_String* _curPresetConf)
{
	memset(g_pMidiLiveWnd->m_lastFXPresetMsg[0], -1, SNM_LIVECFG_MAX_PRESET_COUNT * sizeof(int));
	memset(g_pMidiLiveWnd->m_lastFXPresetMsg[1], -1, SNM_LIVECFG_MAX_PRESET_COUNT * sizeof(int));

	int fxCount = TrackFX_GetCount(_tr);
	if(!fxCount) {
		AddToMenu(*_menu, "(No FX on track)", 0, -1, false, MF_DISABLED);
		return false;
	}
	
	SNM_FXSummaryParser p(_tr);
	WDL_PtrList<SNM_FXSummary>* summaries = p.GetSummaries();
	if (summaries && summaries->GetSize())
	{
		char fxName[512];
		int msgCpt = 0;
		//JFB TODO: check max. msgCpt value..
		for(int i = 0; i < fxCount; i++) 
		{
			SNM_FXSummary* sum = summaries->Get(i);
			if(TrackFX_GetFXName(_tr, i, fxName, 512))
			{
				HMENU fxSubMenu = CreatePopupMenu();
				WDL_PtrList_DeleteOnDestroy<WDL_String> names;
				int presetCount = getPresetNames(sum->m_type.Get(), sum->m_realName.Get(), &names);
				if (presetCount)
				{
					int curSel = GetSelPresetFromConf(i, _curPresetConf, presetCount);
					AddToMenu(fxSubMenu, "None (unchanged)", SNM_LIVECFG_SET_PRESETS_MSG + msgCpt, -1, false, !curSel ? MFS_CHECKED : MFS_UNCHECKED);
					g_pMidiLiveWnd->m_lastFXPresetMsg[0][msgCpt] = i; 
					g_pMidiLiveWnd->m_lastFXPresetMsg[1][msgCpt++] = 0; 
					for(int j = 0; j < presetCount; j++) {
						AddToMenu(fxSubMenu, names.Get(j)->Get(), SNM_LIVECFG_SET_PRESETS_MSG + msgCpt, -1, false, ((j+1)==curSel) ? MFS_CHECKED : MFS_UNCHECKED);
						g_pMidiLiveWnd->m_lastFXPresetMsg[0][msgCpt] = i; 
						g_pMidiLiveWnd->m_lastFXPresetMsg[1][msgCpt++] = j+1; 
					}
				}

				AddSubMenu(*_menu, fxSubMenu, fxName, -1, presetCount ? MFS_ENABLED : MF_DISABLED);
			}
		}
	}
	return true;
}

///////////////////////////////////////////////////////////////////////////////
// SnM_MidiLiveView
///////////////////////////////////////////////////////////////////////////////

SnM_MidiLiveView::SnM_MidiLiveView(HWND hwndList, HWND hwndEdit)
:SWS_ListView(hwndList, hwndEdit, 7, g_midiLiveCols, "S&M - Live Configs View State", false)
{}

void SnM_MidiLiveView::GetItemText(LPARAM item, int iCol, char* str, int iStrMax)
{
	MidiLiveItem* pItem = (MidiLiveItem*)item;
	if (pItem)
	{
		switch (iCol)
		{
			case 0:
				_snprintf(str, iStrMax, "%03d %c", pItem->m_cc, pItem->m_cc == g_liveConfigs.Get()->m_lastMIDIVal[g_configId] ? '*' : ' ');
				break;
			case 1:
				lstrcpyn(str, pItem->m_desc.Get(), iStrMax);
				break;
			case 2:
			{
				bool ok = false;
				if (pItem->m_track) {
					char* name = (char*)GetSetMediaTrackInfo(pItem->m_track, "P_NAME", NULL);
					if (name)
					{
						_snprintf(str, iStrMax, "[%d] \"%s\"", CSurf_TrackToID(pItem->m_track, false), name);
						ok = true;
					}
				}
				if (!ok)
					lstrcpyn(str, "", iStrMax);
				break;
			}
			case 3:
				lstrcpyn(str, pItem->m_trTemplate.Get(), iStrMax);
				break;
			case 4:
				lstrcpyn(str, pItem->m_fxChain.Get(), iStrMax);
				break;
/*Preset
			case 5:
			{
				WDL_String renderConf;
				RenderPresetConf(&(pItem->m_presets), &renderConf);
				lstrcpyn(str, renderConf.Get(), iStrMax);
				break;
			}
*/
			//JFB TODO? kbd_getTextFromCmd, tooltip?
			case 5:
				lstrcpyn(str, pItem->m_onAction.Get(), iStrMax);
				break;
			case 6:
				lstrcpyn(str, pItem->m_offAction.Get(), iStrMax);
				break;
			default:
				break;
		}
	}
}

void SnM_MidiLiveView::SetItemText(LPARAM item, int iCol, const char* str)
{
	MidiLiveItem* pItem = (MidiLiveItem*)item;
	if (pItem)
	{
		if (iCol==1)
		{
			// Limit the desc. size (for RPP files)
			pItem->m_desc.Set(str);
			pItem->m_desc.Ellipsize(32,32);
			Update();
			Undo_OnStateChangeEx(SNM_LIVECFG_UNDO_STR, UNDO_STATE_MISCCFG, -1);
		}
		else if (iCol==5 || iCol == 6)
		{
			if (*str != 0 && !NamedCommandLookup(str)) {
				MessageBox(GetParent(m_hwndList), "Error: this action ID (or custom ID) doesn't exists.", "S&M - Live Configs", MB_OK);
				return;
			}

			if (iCol==5)
				pItem->m_onAction.Set(str);
			else
				pItem->m_offAction.Set(str);

			Update();
			Undo_OnStateChangeEx(SNM_LIVECFG_UNDO_STR, UNDO_STATE_MISCCFG, -1);
		}
	}
}

void SnM_MidiLiveView::GetItemList(WDL_TypedBuf<LPARAM>* pBuf)
{
	pBuf->Resize(NB_CC_VALUES);
	for (int i = 0; i < pBuf->GetSize(); i++)
	{
		WDL_PtrList<MidiLiveItem>* configs = g_liveCCConfigs.Get()->Get(g_configId);
		if (configs)
			pBuf->Get()[i] = (LPARAM)configs->Get(i);
	}
}

void SnM_MidiLiveView::OnItemDblClk(LPARAM item, int iCol)
{
	MidiLiveItem* pItem = (MidiLiveItem*)item;
	if (pItem)
	{
		switch(iCol)
		{
			case 0:
			{
				g_pMidiLiveWnd->OnCommand(SNM_LIVECFG_PERFORM_MSG, item);
				break;
			}
			case 3:
				if (pItem->m_track)
					g_pMidiLiveWnd->OnCommand(SNM_LIVECFG_LOAD_TRACK_TEMPLATE_MSG, item);
				break;
			case 4:
				if (pItem->m_track)
					g_pMidiLiveWnd->OnCommand(SNM_LIVECFG_LOAD_FXCHAIN_MSG, item);
				break;
			default:
				break;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
// SnM_MidiLiveWnd
///////////////////////////////////////////////////////////////////////////////

SnM_MidiLiveWnd::SnM_MidiLiveWnd()
:SWS_DockWnd(IDD_SNM_MIDI_LIVE, "Live Configs", 30009, SWSGetCommandID(OpenMidiLiveView))
{
	if (m_bShowAfterInit)
		Show(false, false);
}

void SnM_MidiLiveWnd::CSurfSetTrackListChange() {
	// we use a ScheduledJob because of possible multi-notifs
	SNM_LiveCfg_TLChangeSchedJob* job = new SNM_LiveCfg_TLChangeSchedJob();
	AddOrReplaceScheduledJob(job);
}

void SnM_MidiLiveWnd::CSurfSetTrackTitle() {
	FillComboInputTrack();
	Update();
}

void SnM_MidiLiveWnd::FillComboInputTrack() {
	m_cbInputTr.Empty();
	m_cbInputTr.AddItem("None");
	for (int i=1; i <= GetNumTracks(); i++)
	{
		WDL_String ellips;
		char* name = (char*)GetSetMediaTrackInfo(CSurf_TrackFromID(i,false), "P_NAME", NULL);
		ellips.SetFormatted(64, "[%d] \"%s\"", i, name);
		ellips.Ellipsize(24, 24);
		m_cbInputTr.AddItem(ellips.Get());
	}
}

void SnM_MidiLiveWnd::SelectByCCValue(int _configId, int _cc)
{
	if (_configId == g_configId)
	{
		SWS_ListView* lv = m_pLists.Get(0);
		HWND hList = lv ? lv->GetHWND() : NULL;
		if (lv && hList) // this can be called when the view is closed!
		{
			for (int i = 0; i < lv->GetListItemCount(); i++)
			{
				MidiLiveItem* item = (MidiLiveItem*)lv->GetListItem(i);
				if (item && item->m_cc == _cc) 
				{
					ListView_SetItemState(hList, -1, 0, LVIS_SELECTED);
					ListView_SetItemState(hList, i, LVIS_SELECTED, LVIS_SELECTED); 
					ListView_EnsureVisible(hList, i, true);
					break;
				}
			}
		}
	}
}

void SnM_MidiLiveWnd::Update()
{
	if (m_pLists.GetSize())
		m_pLists.Get(0)->Update();
	m_parentVwnd.RequestRedraw(NULL);
}

void SnM_MidiLiveWnd::OnInitDlg()
{
	m_resize.init_item(IDC_LIST, 0.0, 0.0, 1.0, 1.0);
	m_pLists.Add(new SnM_MidiLiveView(GetDlgItem(m_hwnd, IDC_LIST), GetDlgItem(m_hwnd, IDC_EDIT)));

	// WDL GUI init
	m_parentVwnd.SetRealParent(m_hwnd);

	m_cbConfig.SetID(COMBOID_CONFIG);
	m_cbConfig.SetRealParent(m_hwnd);
	for (int i=0; i < SNM_LIVECFG_NB_CONFIGS; i++)
	{
		char cfg[4] = "";
		sprintf(cfg, "%d", i+1);
		m_cbConfig.AddItem(cfg);
	}
	m_cbConfig.SetCurSel(g_configId);
	m_parentVwnd.AddChild(&m_cbConfig);

	m_btnEnable.SetID(BUTTONID_ENABLE);
	m_btnEnable.SetRealParent(m_hwnd);
	m_parentVwnd.AddChild(&m_btnEnable);

	m_btnAutoRcv.SetID(BUTTONID_AUTO_RCV);
	m_btnAutoRcv.SetRealParent(m_hwnd);
	m_parentVwnd.AddChild(&m_btnAutoRcv);

	m_btnMuteOthers.SetID(BUTTONID_MUTE_OTHERS);
	m_btnMuteOthers.SetRealParent(m_hwnd);
	m_parentVwnd.AddChild(&m_btnMuteOthers);

	m_cbInputTr.SetID(COMBOID_INPUT_TRACK);
	m_cbInputTr.SetRealParent(m_hwnd);
	FillComboInputTrack();
	m_parentVwnd.AddChild(&m_cbInputTr);

	Update();
}

void SnM_MidiLiveWnd::OnCommand(WPARAM wParam, LPARAM lParam)
{
	int x=0;
	MidiLiveItem* item = (MidiLiveItem*)m_pLists.Get(0)->EnumSelected(&x);
	switch (wParam)
	{
		case SNM_LIVECFG_CLEAR_CC_ROW_MSG:
		{
			bool updt = false;
			while(item) {
				item->Clear();
				updt = true;
				item = (MidiLiveItem*)m_pLists.Get(0)->EnumSelected(&x);
			}
			if (updt) {
				Update();
				Undo_OnStateChangeEx(SNM_LIVECFG_UNDO_STR, UNDO_STATE_MISCCFG, -1);
			}
			break;
		}
		case SNM_LIVECFG_PERFORM_MSG:
			if (item) {
				// Immediate job
				SNM_MidiLiveScheduledJob* job = 
					new SNM_MidiLiveScheduledJob(g_configId, 0, g_configId, item->m_cc, -1, 0, g_hwndParent);
				AddOrReplaceScheduledJob(job);
			}
			break;
		case SNM_LIVECFG_EDIT_DESC_MSG:
			if (item) 
				m_pLists.Get(0)->EditListItem((LPARAM)item, 1);
			break;
		case SNM_LIVECFG_CLEAR_DESC_MSG:
		{
			bool updt = false;
			while(item) {
				item->m_desc.Set("");
				updt = true;
				item = (MidiLiveItem*)m_pLists.Get(0)->EnumSelected(&x);
			}
			if (updt) {
				Update();
				Undo_OnStateChangeEx(SNM_LIVECFG_UNDO_STR, UNDO_STATE_MISCCFG, -1);
			}
			break;
		}
		case SNM_LIVECFG_LOAD_TRACK_TEMPLATE_MSG:
		{
			bool updt = false;
			if (item)
			{
				char filename[BUFFER_SIZE];
				if (BrowseResourcePath("S&M - Load track template", "TrackTemplates", "REAPER Track Template (*.RTrackTemplate)\0*.RTrackTemplate\0", filename, BUFFER_SIZE))
				{
					while(item) {
						item->m_trTemplate.Set(filename);
						item->m_fxChain.Set("");
						item->m_presets.Set("");
						updt = true;
						item = (MidiLiveItem*)m_pLists.Get(0)->EnumSelected(&x);
					}
				}
			}
			if (updt) {
				Update();
				Undo_OnStateChangeEx(SNM_LIVECFG_UNDO_STR, UNDO_STATE_MISCCFG, -1);
			}
			break;
		}
		case SNM_LIVECFG_CLEAR_TRACK_TEMPLATE_MSG:
		{
			bool updt = false;
			while(item) {
				item->m_trTemplate.Set("");
				updt = true;
				item = (MidiLiveItem*)m_pLists.Get(0)->EnumSelected(&x);
			}
			if (updt) {
				Update();
				Undo_OnStateChangeEx(SNM_LIVECFG_UNDO_STR, UNDO_STATE_MISCCFG, -1);
			}
			break;
		}
		case SNM_LIVECFG_LOAD_FXCHAIN_MSG:
		{
			bool updt = false;
			if (item)
			{
				char filename[BUFFER_SIZE];
				if (BrowseResourcePath("S&M - FX Chain", "FXChains", "REAPER FX Chain (*.RfxChain)\0*.RfxChain\0", filename, BUFFER_SIZE))
				{
					while(item) {
						item->m_fxChain.Set(filename);
						item->m_trTemplate.Set("");
						item->m_presets.Set("");
						updt = true;
						item = (MidiLiveItem*)m_pLists.Get(0)->EnumSelected(&x);
					}
				}
			}
			if (updt) {
				Update();
				Undo_OnStateChangeEx(SNM_LIVECFG_UNDO_STR, UNDO_STATE_MISCCFG, -1);
			}
			break;
		}
		case SNM_LIVECFG_CLEAR_FXCHAIN_MSG:
		{
			bool updt = false;
			while(item) {
				item->m_fxChain.Set("");
				updt = true;
				item = (MidiLiveItem*)m_pLists.Get(0)->EnumSelected(&x);
			}
			if (updt) {
				Update();
				Undo_OnStateChangeEx(SNM_LIVECFG_UNDO_STR, UNDO_STATE_MISCCFG, -1);
			}
			break;
		}
		case SNM_LIVECFG_EDIT_ON_ACTION_MSG:
			if (item) 
				m_pLists.Get(0)->EditListItem((LPARAM)item, 5);
			break;
		case SNM_LIVECFG_CLEAR_ON_ACTION_MSG:
		{
			bool updt = false;
			while(item) {
				item->m_onAction.Set("");
				updt = true;
				item = (MidiLiveItem*)m_pLists.Get(0)->EnumSelected(&x);
			}
			if (updt) {
				Update();
				Undo_OnStateChangeEx(SNM_LIVECFG_UNDO_STR, UNDO_STATE_MISCCFG, -1);
			}
			break;
		}
		case SNM_LIVECFG_EDIT_OFF_ACTION_MSG:
			if (item) 
				m_pLists.Get(0)->EditListItem((LPARAM)item, 6);
			break;
		case SNM_LIVECFG_CLEAR_OFF_ACTION_MSG:
		{
			bool updt = false;
			while(item) {
				item->m_offAction.Set("");
				updt = true;
				item = (MidiLiveItem*)m_pLists.Get(0)->EnumSelected(&x);
			}
			if (updt) {
				Update();
				Undo_OnStateChangeEx(SNM_LIVECFG_UNDO_STR, UNDO_STATE_MISCCFG, -1);
			}
			break;
		}
		case SNM_LIVECFG_CLEAR_TRACK_MSG:
		{
			bool updt = false;
			while(item) {
				item->m_track= NULL;
				item->m_trTemplate.Set("");
				item->m_fxChain.Set("");
				item->m_presets.Set("");
				updt = true;
				item = (MidiLiveItem*)m_pLists.Get(0)->EnumSelected(&x);
			}
			if (updt) {
				Update();
				Undo_OnStateChangeEx(SNM_LIVECFG_UNDO_STR, UNDO_STATE_MISCCFG, -1);
			}
			break;
		}
		case SNM_LIVECFG_CLEAR_PRESETS_MSG:
		{
			bool updt = false;
			while(item) {
				item->m_presets.Set("");
				updt = true;
				item = (MidiLiveItem*)m_pLists.Get(0)->EnumSelected(&x);
			}
			if (updt) {
				Update();
				Undo_OnStateChangeEx(SNM_LIVECFG_UNDO_STR, UNDO_STATE_MISCCFG, -1);
			}
			break;
		}
		default:
		{
			if (wParam >= SNM_LIVECFG_SET_TRACK_MSG && wParam < SNM_LIVECFG_CLEAR_TRACK_MSG) 
			{
				bool updt = false;
				while(item)
				{
					item->m_track = CSurf_TrackFromID((int)wParam - SNM_LIVECFG_SET_TRACK_MSG, false);
					if (!(item->m_track)) {
						item->m_fxChain.Set("");
						item->m_trTemplate.Set("");
					}
					item->m_presets.Set("");
					updt = true;
					item = (MidiLiveItem*)m_pLists.Get(0)->EnumSelected(&x);
				}
				if (updt) {
					Update();
					Undo_OnStateChangeEx(SNM_LIVECFG_UNDO_STR, UNDO_STATE_MISCCFG, -1);
				}
			}
/*Preset
			else if (wParam >= SNM_LIVECFG_SET_PRESETS_MSG && wParam < SNM_LIVECFG_CLEAR_PRESETS_MSG) 
			{
				bool updt = false;
				while(item)
				{
					int fx = m_lastFXPresetMsg[0][(int)wParam - SNM_LIVECFG_SET_PRESETS_MSG];
					int preset = m_lastFXPresetMsg[1][(int)wParam - SNM_LIVECFG_SET_PRESETS_MSG];
					UpdatePresetConf(fx+1, preset, &(item->m_presets));
					item->m_fxChain.Set("");
					item->m_trTemplate.Set("");
					updt = true;
					item = (MidiLiveItem*)m_pLists.Get(0)->EnumSelected(&x);
				}
				if (updt) {
					Update();
					Undo_OnStateChangeEx(SNM_LIVECFG_UNDO_STR, UNDO_STATE_MISCCFG, -1);
				}
			}
*/
			// WDL GUI
			else if (HIWORD(wParam)==CBN_SELCHANGE && LOWORD(wParam)==COMBOID_INPUT_TRACK)
			{
				if (!m_cbInputTr.GetCurSel())
					g_liveConfigs.Get()->m_inputTr[g_configId] = NULL;
				else
					g_liveConfigs.Get()->m_inputTr[g_configId] = CSurf_TrackFromID(m_cbInputTr.GetCurSel(), false);
				m_parentVwnd.RequestRedraw(NULL);
			}
			else if (HIWORD(wParam)==CBN_SELCHANGE && LOWORD(wParam)==COMBOID_CONFIG)
			{
				g_configId = m_cbConfig.GetCurSel();
				Update();
			}
			else
				Main_OnCommand((int)wParam, (int)lParam);

			break;
		}
	}
}

HMENU SnM_MidiLiveWnd::OnContextMenu(int x, int y)
{
	HMENU hMenu = NULL;
	int iCol;
	MidiLiveItem* item = (MidiLiveItem*)m_pLists.Get(0)->GetHitItem(x, y, &iCol);
	if (item)
	{
		switch(iCol)
		{
			case 0:
			{
				hMenu = CreatePopupMenu();
				AddToMenu(hMenu, "Perform", SNM_LIVECFG_PERFORM_MSG);
				break;
			}
			case 1:
			{
				hMenu = CreatePopupMenu();
				AddToMenu(hMenu, "Clear", SNM_LIVECFG_CLEAR_DESC_MSG);
				AddToMenu(hMenu, "Edit", SNM_LIVECFG_EDIT_DESC_MSG);
				break;
			}
			case 2:
			{
				hMenu = CreatePopupMenu();
				AddToMenu(hMenu, "Clear", SNM_LIVECFG_CLEAR_TRACK_MSG);
				for (int i=1; i <= GetNumTracks(); i++)
				{
					char str[64] = "";
					char* name = (char*)GetSetMediaTrackInfo(CSurf_TrackFromID(i,false), "P_NAME", NULL);
					_snprintf(str, 64, "[%d] \"%s\"", i, name);
					AddToMenu(hMenu, str, SNM_LIVECFG_SET_TRACK_MSG + i);
				}
				break;
			}
			case 3:
			{
				hMenu = CreatePopupMenu();
				if (item->m_track) {
					AddToMenu(hMenu, "Clear", SNM_LIVECFG_CLEAR_TRACK_TEMPLATE_MSG);
					AddToMenu(hMenu, "Load track template...", SNM_LIVECFG_LOAD_TRACK_TEMPLATE_MSG);
				}
				else AddToMenu(hMenu, "(No track)", 0, -1, false, MF_DISABLED);
				break;
			}
			case 4:
			{
				hMenu = CreatePopupMenu();
				if (item->m_track) {
					if (!item->m_trTemplate.GetLength()) {
						AddToMenu(hMenu, "Clear", SNM_LIVECFG_CLEAR_FXCHAIN_MSG);
						AddToMenu(hMenu, "Load FX Chain...", SNM_LIVECFG_LOAD_FXCHAIN_MSG);
					}
					else AddToMenu(hMenu, "(Track template overrides)", 0, -1, false, MF_DISABLED);
				}
				else AddToMenu(hMenu, "(No track)", 0, -1, false, MF_DISABLED);
				break;
			}
/*Preset
			case 5:
			{
				hMenu = CreatePopupMenu();
				if (item->m_track) {
					if (item->m_trTemplate.GetLength() && !item->m_fxChain.GetLength())
						AddToMenu(hMenu, "(Track template overrides)", 0, -1, false, MFS_DISABLED);
					else if (item->m_fxChain.GetLength())
						AddToMenu(hMenu, "(FX Chain overrides)", 0, -1, false, MFS_DISABLED);
					else {
						AddToMenu(hMenu, "Clear all presets (unchanged)", SNM_LIVECFG_CLEAR_PRESETS_MSG);
						AddToMenu(hMenu, SWS_SEPARATOR, 0);
						AddFXSubMenu(&hMenu, item->m_track, &(item->m_presets));
					}
				}
				else AddToMenu(hMenu, "(No track)", 0, -1, false, MFS_DISABLED);
				break;
			}
*/
			case 5:
			{
				hMenu = CreatePopupMenu();
				AddToMenu(hMenu, "Clear", SNM_LIVECFG_CLEAR_ON_ACTION_MSG);
				AddToMenu(hMenu, "Edit", SNM_LIVECFG_EDIT_ON_ACTION_MSG);
				break;
			}
			case 6:
			{
				hMenu = CreatePopupMenu();
				AddToMenu(hMenu, "Clear", SNM_LIVECFG_CLEAR_OFF_ACTION_MSG);
				AddToMenu(hMenu, "Edit", SNM_LIVECFG_EDIT_OFF_ACTION_MSG);
				break;
			}
		}
	}
	return hMenu;
}

void SnM_MidiLiveWnd::OnDestroy() 
{
	m_cbConfig.Empty();
	m_cbInputTr.Empty();
	m_parentVwnd.RemoveAllChildren(false);
}

int SnM_MidiLiveWnd::OnKey(MSG* msg, int iKeyState) 
{
	if (msg->message == WM_KEYDOWN && msg->wParam == VK_DELETE && !iKeyState) {
		OnCommand(SNM_LIVECFG_CLEAR_CC_ROW_MSG, 0);
		return 1;
	}
	return 0;
}


static void DrawControls(WDL_VWnd_Painter *_painter, RECT _r, WDL_VWnd* _parentVwnd)
{
	if (!g_pMidiLiveWnd) // SWS Can't draw before wnd initialized - why isn't this a member func??
		return;			  // JFB TODO yes, I was planing a kind of SNM_Wnd at some point..
	
	int xo=0, yo=0, sz;
    LICE_IBitmap *bm = _painter->GetBuffer(&xo,&yo);
	if (bm)
	{
		ColorTheme* ct = (ColorTheme*)GetColorThemeStruct(&sz);

		static LICE_IBitmap *logo=  NULL;
		if (!logo)
		{
#ifdef _WIN32
			logo = LICE_LoadPNGFromResource(g_hInst,IDB_SNM,NULL);
#else
			// SWS doesn't work, sorry. :( logo =  LICE_LoadPNGFromNamedResource("SnM.png",NULL);
			logo = NULL;
#endif
		}

		static LICE_CachedFont tmpfont;
		if (!tmpfont.GetHFont())
		{
			LOGFONT lf = {
			  14,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
				OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,
			  #ifdef _WIN32
			  "MS Shell Dlg"
			  #else
			  "Arial"
			  #endif
			};
			if (ct) 
				lf = ct->mediaitem_font;
			tmpfont.SetFromHFont(CreateFontIndirect(&lf),LICE_FONT_FLAG_OWNS_HFONT);                 
		}
		tmpfont.SetBkMode(TRANSPARENT);
		if (ct)	tmpfont.SetTextColor(LICE_RGBA_FROMNATIVE(ct->main_text,255));
		else tmpfont.SetTextColor(LICE_RGBA(255,255,255,255));

		int x0=_r.left+10, y0=_r.top+5;

		// Dropdowns
		WDL_VirtualComboBox* cbVwnd = (WDL_VirtualComboBox*)_parentVwnd->GetChildByID(COMBOID_CONFIG);
		if (cbVwnd)
		{
			RECT tr={x0,y0,x0+40,y0+25};
			tmpfont.DrawText(bm, "Config:", -1, &tr, DT_LEFT | DT_VCENTER);
			x0 = tr.right+5;

			RECT tr2={x0,y0+3,x0+37,y0+25-2};
			x0 = tr2.right+5;
			cbVwnd->SetPosition(&tr2);
			cbVwnd->SetFont(&tmpfont);
			cbVwnd->SetCurSel(g_configId);
		}

		WDL_VirtualIconButton* btn = (WDL_VirtualIconButton*)_parentVwnd->GetChildByID(BUTTONID_ENABLE);
		if (btn)
		{
			x0 += 5;
			RECT tr={x0,y0+5,x0+15,y0+20};
			x0 = tr.right+5;
			btn->SetPosition(&tr);
			btn->SetCheckState(g_liveConfigs.Get()->m_enable[g_configId]);

			RECT tr2={x0,y0,x0+40,y0+25};
			tmpfont.DrawText(bm, "Enable", -1, &tr2, DT_LEFT | DT_VCENTER);
			x0 = tr2.right+5;
		}
/*JFB not released
		btn = (WDL_VirtualIconButton*)_parentVwnd->GetChildByID(BUTTONID_AUTO_RCV);
		if (btn)
		{
			x0 += 5;
			RECT tr={x0,y0+5,x0+15,y0+20};
			x0 = tr.right+5;
			btn->SetPosition(&tr);
			btn->SetCheckState(g_pMidiLiveWnd->m_autoRcv[g_configId]);

			RECT tr2={x0,y0,x0+120,y0+25};
			int h = tmpfont.DrawText(bm, "Auto receive input track", -1, &tr2, DT_LEFT | DT_VCENTER);
			x0 = tr2.right+5;
		}
*/
		btn = (WDL_VirtualIconButton*)_parentVwnd->GetChildByID(BUTTONID_MUTE_OTHERS);
		if (btn)
		{
			x0 += 5;
			RECT tr={x0,y0+5,x0+15,y0+20};
			x0 = tr.right+5;
			btn->SetPosition(&tr);
			btn->SetCheckState(g_liveConfigs.Get()->m_muteOthers[g_configId]);

			RECT tr2={x0,y0,x0+120,y0+25};
			tmpfont.DrawText(bm, "Mute all but active track", -1, &tr2, DT_LEFT | DT_VCENTER);
			x0 = tr2.right+5;
		}

		cbVwnd = (WDL_VirtualComboBox*)_parentVwnd->GetChildByID(COMBOID_INPUT_TRACK);
		if (cbVwnd)
		{
			x0 += 5;
			RECT tr={x0,y0,x0+55,y0+25};
			tmpfont.DrawText(bm, "Input track:", -1, &tr, DT_LEFT | DT_VCENTER);
			x0 = tr.right+5;

			RECT tr2={x0,y0+3,x0+125,y0+25-2};
			x0 = tr2.right+5;
			cbVwnd->SetPosition(&tr2);
			cbVwnd->SetFont(&tmpfont);
			int sel=0;
			for (int i=1; i <= GetNumTracks(); i++) {
				if (g_liveConfigs.Get()->m_inputTr[g_configId] == CSurf_TrackFromID(i,false)) {
					sel = i;
					break;
				}
			}
			cbVwnd->SetCurSel(sel);
		}

		_painter->PaintVirtWnd(_parentVwnd, 0);

		if (logo && (_r.right - _r.left) > (x0+logo->getWidth()))
			LICE_Blit(bm,logo,_r.right-logo->getWidth()-8,y0+3,NULL,0.125f,LICE_BLIT_MODE_ADD|LICE_BLIT_USE_ALPHA);
	}
}

int SnM_MidiLiveWnd::OnUnhandledMsg(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_PAINT:
		{
			RECT r; int sz;
			GetClientRect(m_hwnd,&r);	
			m_parentVwnd.SetPosition(&r);

			ColorTheme* ct = (ColorTheme*)GetColorThemeStruct(&sz);
			if (ct)	m_vwnd_painter.PaintBegin(m_hwnd, ct->tracklistbg_color);
			else m_vwnd_painter.PaintBegin(m_hwnd, LICE_RGBA(0,0,0,255));
			DrawControls(&m_vwnd_painter,r, &m_parentVwnd);
			m_vwnd_painter.PaintEnd();
		}
		break;

		case WM_LBUTTONDOWN:
		{
			SetFocus(g_pMidiLiveWnd->GetHWND());
			WDL_VWnd *w = m_parentVwnd.VirtWndFromPoint(GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam));
			MidiLiveConfig* lc = g_liveConfigs.Get();
			if (w && lc) 
			{
				if (w == &m_btnEnable) {
					lc->m_enable[g_configId] = !(lc->m_enable[g_configId]);
					lc->m_lastDeactivateCmd[g_configId][0] = -1;
					if (lc->m_lastMIDIVal[g_configId] != -1) {
						lc->m_lastMIDIVal[g_configId] = -1;
						Update();
					}
				}
				else if (w == &m_btnAutoRcv)
					lc->m_autoRcv[g_configId] = !(lc->m_autoRcv[g_configId]);
				else if (w == &m_btnMuteOthers)
					lc->m_muteOthers[g_configId] = !(lc->m_muteOthers[g_configId]);

				w->OnMouseDown(GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam));
			}
		}
		break;

		case WM_LBUTTONUP:
		{
			int x = GET_X_LPARAM(lParam);
			int y = GET_Y_LPARAM(lParam);
			WDL_VWnd *w = m_parentVwnd.VirtWndFromPoint(x,y);
			if (w) w->OnMouseUp(GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam));
		}
		break;

		case WM_MOUSEMOVE:
		{
			int x = GET_X_LPARAM(lParam);
			int y = GET_Y_LPARAM(lParam);
			WDL_VWnd *w = m_parentVwnd.VirtWndFromPoint(x,y);
			if (w) w->OnMouseMove(GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam));
		}
		break;
	}
	return 0;
}


///////////////////////////////////////////////////////////////////////////////

void InitModel()
{
	g_liveConfigs.Get()->Clear(); // then lazy init
	g_liveCCConfigs.Get()->Empty(true);
	for (int i=0; i < SNM_LIVECFG_NB_CONFIGS; i++) 
	{
		g_liveCCConfigs.Get()->Add(new WDL_PtrList_DeleteOnDestroy<MidiLiveItem>);
		for (int j = 0; j < NB_CC_VALUES; j++)
			g_liveCCConfigs.Get()->Get(i)->Add(new MidiLiveItem(j, "", NULL, "", "", "", "", ""));
	}
}

void SNM_LiveCfg_TLChangeSchedJob::Perform()
{
	if (g_pMidiLiveWnd && g_liveCCConfigs.IsValid(Enum_Projects(-1, NULL, 0)))
	{
		g_pMidiLiveWnd->FillComboInputTrack();
		g_pMidiLiveWnd->Update();
	}
	else
	{
		// PiPs go here..
	}
}


///////////////////////////////////////////////////////////////////////////////

static bool ProcessExtensionLine(const char *line, ProjectStateContext *ctx, bool isUndo, struct project_config_extension_t *reg)
{
	LineParser lp(false);
	if (lp.parse(line) || lp.getnumtokens() < 2)
		return false;

	if (!strcmp(lp.gettoken_str(0), "<S&M_MIDI_LIVE"))
	{
		GUID g;	
		int configId = lp.gettoken_int(1) - 1;
		MidiLiveConfig* lc = g_liveConfigs.Get();

		if (lc)
		{
			lc->m_enable[configId] = lp.gettoken_int(2);
			lc->m_autoRcv[configId] = lp.gettoken_int(3);
			lc->m_muteOthers[configId] = lp.gettoken_int(4);
			stringToGuid(lp.gettoken_str(5), &g);
			lc->m_inputTr[configId] = GuidToTrack(&g);
		}

		WDL_PtrList<MidiLiveItem>* ccConfigs = g_liveCCConfigs.Get()->Get(configId);
		if (ccConfigs)
		{
			char linebuf[4096];
			while(true)
			{
				if (!ctx->GetLine(linebuf,sizeof(linebuf)) && !lp.parse(linebuf))
				{
					if (lp.gettoken_str(0)[0] == '>')
						break;						
					else
					{
						MidiLiveItem* item = ccConfigs->Get(lp.gettoken_int(0));
						if (item)
						{
							item->m_cc = lp.gettoken_int(0);
							item->m_desc.Set(lp.gettoken_str(1));
							stringToGuid(lp.gettoken_str(2), &g);
							item->m_track = GuidToTrack(&g);
							item->m_trTemplate.Set(lp.gettoken_str(3));
							item->m_fxChain.Set(lp.gettoken_str(4));
							item->m_presets.Set(lp.gettoken_str(5));
							item->m_onAction.Set(lp.gettoken_str(6));
							item->m_offAction.Set(lp.gettoken_str(7));
						}
					}
				}
				else
					break;
			}
		}

		if (g_pMidiLiveWnd)
			g_pMidiLiveWnd->Update();

		return true;
	}
	return false;
}

static void SaveExtensionConfig(ProjectStateContext *ctx, bool isUndo, struct project_config_extension_t *reg)
{
	if (!g_liveCCConfigs.IsValid(Enum_Projects(-1, NULL, 0)))
		return; 

	char curLine[SNM_MAX_CHUNK_LINE_LENGTH] = "", strId[128] = "";
	GUID g; 
	bool firstCfg = true;
	for (int i=0; i < g_liveCCConfigs.Get()->GetSize(); i++) 
	{
	  for (int j = 0; j < g_liveCCConfigs.Get()->Get(i)->GetSize(); j++)
	  {
			MidiLiveItem* item = g_liveCCConfigs.Get()->Get(i)->Get(j);
			if (item && !item->IsDefault()) // avoid a bunch of useless data in RPP files!
			{
				if (firstCfg)
				{
					firstCfg = false;
					MidiLiveConfig* lc = g_liveConfigs.Get();
					if (lc)
					{
						if (lc->m_inputTr[i] && CSurf_TrackToID(lc->m_inputTr[i], false)) 
							g = *(GUID*)GetSetMediaTrackInfo(lc->m_inputTr[i], "GUID", NULL);
						else 
							g = GUID_NULL;
						guidToString(&g, strId);

						_snprintf(curLine, SNM_MAX_CHUNK_LINE_LENGTH, "<S&M_MIDI_LIVE %d %d %d %d \"%s\"", 
							i+1,
							lc->m_enable[i],
							lc->m_autoRcv[i],
							lc->m_muteOthers[i],
							strId);
						ctx->AddLine(curLine);
					}
				}

				if (item->m_track && CSurf_TrackToID(item->m_track, false)) 
					g = *(GUID*)GetSetMediaTrackInfo(item->m_track, "GUID", NULL);
				else 
					g = GUID_NULL;
				guidToString(&g, strId);

				_snprintf(curLine, SNM_MAX_CHUNK_LINE_LENGTH, "%d \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\"", 
					item->m_cc, 
					item->m_desc.Get(), 
					strId, 
					item->m_trTemplate.Get(), 
					item->m_fxChain.Get(), 
					item->m_presets.Get(), 
					item->m_onAction.Get(), 
					item->m_offAction.Get());
				ctx->AddLine(curLine);
			}
		}

		if (!firstCfg)
			ctx->AddLine(">");
		firstCfg = true;
	}
}

static void BeginLoadProjectState(bool isUndo, struct project_config_extension_t *reg)
{
	g_liveConfigs.Cleanup();
	g_liveCCConfigs.Cleanup();
	if (!g_liveCCConfigs.Get()->GetSize())
	{
		g_liveConfigs.Get()->Clear();
		g_liveCCConfigs.Get()->Empty(true);
		InitModel();
	}
}

static project_config_extension_t g_projectconfig = {
	ProcessExtensionLine, SaveExtensionConfig, BeginLoadProjectState, NULL
};

static void menuhook(const char* menustr, HMENU hMenu, int flag)
{
	if (!strcmp(menustr, "Main view") && !flag)
	{
		int cmd = NamedCommandLookup("_S&M_SHOWMIDILIVE");
		if (cmd > 0) {
			int afterCmd = NamedCommandLookup("_SWSCONSOLE");
			AddToMenu(hMenu, "S&&M Live Configs", cmd, afterCmd > 0 ? afterCmd : 40075);
		}
	}
}

int MidiLiveViewInit()
{
	InitModel();

	g_pMidiLiveWnd = new SnM_MidiLiveWnd();

	if (!g_pMidiLiveWnd || 
//JFB not in main 'view' menu		!plugin_register("hookcustommenu", (void*)menuhook) ||
		!plugin_register("projectconfig",&g_projectconfig))
		return 0;

	return 1;
}

void MidiLiveViewExit() {
	delete g_pMidiLiveWnd;
	g_pMidiLiveWnd = NULL;
}

void OpenMidiLiveView(COMMAND_T*) {
	g_pMidiLiveWnd->Show(true, true);
}

bool IsMidiLiveViewEnabled(COMMAND_T*){
	return g_pMidiLiveWnd->IsValidWindow();
}


///////////////////////////////////////////////////////////////////////////////
// CC processing
///////////////////////////////////////////////////////////////////////////////

// val/valhw are used for midi stuff.
// val=[0..127] and valhw=-1 (midi CC),
// valhw >=0 (midi pitch (valhw | val<<7)),
// relmode absolute (0) or 1/2/3 for relative adjust modes
void ApplyLiveConfig(MIDI_COMMAND_T* _ct, int _val, int _valhw, int _relmode, HWND _hwnd) 
{
	// Absolute CC only
	if (!_relmode && _valhw < 0)
	{
		// Avoid to stuck REPAER when a bunch of CCs are received (which is the standard case 
		// with HW knobs, faders, ..) but just process the last "stable" one
		// => we do this thanks to SNM_ScheduledJob
		SNM_MidiLiveScheduledJob* job = 
			new SNM_MidiLiveScheduledJob((int)_ct->user, 250, (int)_ct->user, _val, _valhw, _relmode, _hwnd);
		AddOrReplaceScheduledJob(job);
	}
}

// Here we go
void SNM_MidiLiveScheduledJob::Perform()
{
	MidiLiveConfig* lc = g_liveConfigs.Get();
	if (!lc || m_val == lc->m_lastMIDIVal[m_cfgId]) 
		return;

	if (lc->m_enable[m_cfgId])
	{
		// refresh last executed CC value conf.
		lc->m_lastMIDIVal[m_cfgId] = m_val;

		// Run desactivate action of previous CC
		// if one, the previous CC's track still selected
		if (lc->m_lastDeactivateCmd[m_cfgId][0] > 0)
		{
			if (!KBD_OnMainActionEx(lc->m_lastDeactivateCmd[m_cfgId][0], lc->m_lastDeactivateCmd[m_cfgId][1], lc->m_lastDeactivateCmd[m_cfgId][2], lc->m_lastDeactivateCmd[m_cfgId][3], g_hwndParent, NULL))
				Main_OnCommand(lc->m_lastDeactivateCmd[m_cfgId][0],0);
			lc->m_lastDeactivateCmd[m_cfgId][0] = -1;
		}

		MidiLiveItem* cfg = g_liveCCConfigs.Get()->Get(m_cfgId)->Get(m_val);
		if (cfg && cfg->m_track)
		{
			// Mute/unselect all but this
			WDL_PtrList<MediaTrack> cfgTracks;
			for (int i=0; i < g_liveCCConfigs.Get()->Get(m_cfgId)->GetSize(); i++)
			{
				MidiLiveItem* cfgOther = g_liveCCConfigs.Get()->Get(m_cfgId)->Get(i);
				if (/*i != m_val && */cfgOther && cfgOther->m_track && cfgTracks.Find(cfgOther->m_track) == -1)
				{
					cfgTracks.Add(cfgOther->m_track);
					if (cfgOther->m_track != cfg->m_track)
						GetSetMediaTrackInfo(cfgOther->m_track, "I_SELECTED", &g_i0);	

					if (lc->m_muteOthers[m_cfgId])
					{
						if (cfgOther->m_track != cfg->m_track)
							GetSetMediaTrackInfo(cfgOther->m_track, "B_MUTE", &g_bTrue);

						// mute receives from the input track if needed
						if (lc->m_inputTr[m_cfgId] && 
							lc->m_inputTr[m_cfgId] != cfgOther->m_track)
						{
							int rcvIdx=0;
							MediaTrack* rcvTr = (MediaTrack*)GetSetTrackSendInfo(cfgOther->m_track, -1, rcvIdx, "P_SRCTRACK", NULL);
							while(rcvTr)
							{
								if (rcvTr == lc->m_inputTr[m_cfgId])
								{
									bool b = (cfgOther->m_track != cfg->m_track);
									GetSetTrackSendInfo(cfgOther->m_track, -1, rcvIdx, "B_MUTE", &b);
								}
								rcvTr = (MediaTrack*)GetSetTrackSendInfo(cfgOther->m_track, -1, ++rcvIdx, "P_SRCTRACK", NULL);
							}
						}
					}
				}
			}

			// Avoid glitches AFAP: we'll unmute the focused track later (after processing)
			GetSetMediaTrackInfo(cfg->m_track, "B_MUTE", &g_bTrue);

			SNM_ChunkParserPatcher* p = NULL;
			if (cfg->m_trTemplate.GetLength())
			{
				p = new SNM_ChunkParserPatcher(cfg->m_track);
				WDL_String chunk;
				char filename[BUFFER_SIZE];
				GetFullResourcePath("TrackTemplates", cfg->m_trTemplate.Get(), filename, BUFFER_SIZE);
				if (LoadChunk(filename, &chunk) && chunk.GetLength())
					p->SetChunk(&chunk, 1);
			}
			else if (cfg->m_fxChain.GetLength())
			{
				p = new SNM_FXChainTrackPatcher(cfg->m_track);
				WDL_String chunk;
				char filename[BUFFER_SIZE];
				GetFullResourcePath("FXChains", cfg->m_fxChain.Get(), filename, BUFFER_SIZE);
				if (LoadChunk(filename, &chunk) && chunk.GetLength())
					((SNM_FXChainTrackPatcher*)p)->SetFXChain(&chunk);
			}
/*Preset
			else if (cfg->m_presets.GetLength())
			{
				p = new SNM_FXPresetParserPatcher(cfg->m_track);
				((SNM_FXPresetParserPatcher*)p)->SetPresets(&(cfg->m_presets));
			}
*/
/*JFB not released
			if (g_pMidiLiveWnd->m_autoRcv[m_cfgId] && g_pMidiLiveWnd->m_inputTr[m_cfgId]) {			
			}
*/			
			delete p; // + auto commit!!

			// unmute/select
			GetSetMediaTrackInfo(cfg->m_track, "I_SELECTED", &g_i1);
			GetSetMediaTrackInfo(cfg->m_track, "B_MUTE", &g_bFalse);

		} // end of track processing

		// Perform activate action
		if (cfg->m_onAction.GetLength())
		{
			int cmd = NamedCommandLookup(cfg->m_onAction.Get());
			if (cmd > 0)
				if (!KBD_OnMainActionEx(cmd, m_val, m_valhw, m_relmode, g_hwndParent, NULL))
					Main_OnCommand(cmd,0);
		}

		// (just) prepare desactivate action
		if (cfg->m_offAction.GetLength())
		{
			int cmd = NamedCommandLookup(cfg->m_offAction.Get());
			if (cmd > 0)
			{
				lc->m_lastDeactivateCmd[m_cfgId][0] = cmd;
				lc->m_lastDeactivateCmd[m_cfgId][1] = m_val;
				lc->m_lastDeactivateCmd[m_cfgId][2] = m_valhw;
				lc->m_lastDeactivateCmd[m_cfgId][3] = m_relmode;
			}
		}
		else
			lc->m_lastDeactivateCmd[m_cfgId][0] = -1;

		// auto selection
		if (g_pMidiLiveWnd)
		{
			g_pMidiLiveWnd->SelectByCCValue(m_cfgId, m_val);
			g_pMidiLiveWnd->Update();
		}
	}
	else
		lc->m_lastDeactivateCmd[m_cfgId][0] = -1;
}

void SelectProject(MIDI_COMMAND_T* _ct, int _val, int _valhw, int _relmode, HWND _hwnd) 
{
	// Absolute CC only
	if (!_relmode && _valhw < 0)
	{
		// Avoid to stuck REPAER when a bunch of CCs are received (which is the standard case 
		// with HW knobs, faders, ..) but just process the last "stable" one
		// => we do this thanks to SNM_ScheduledJob
		SNM_SelectProjectScheduledJob* job = 
			new SNM_SelectProjectScheduledJob(250, _val, _valhw, _relmode, _hwnd);
		AddOrReplaceScheduledJob(job);
	}
}

void SNM_SelectProjectScheduledJob::Perform()
{
	ReaProject* proj = Enum_Projects(m_val, NULL, 0);
	if (proj)
		SelectProjectInstance(proj);
}
