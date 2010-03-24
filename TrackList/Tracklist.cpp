/******************************************************************************
/ Tracklist.cpp
/
/ Copyright (c) 2010 Tim Payne (SWS)
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


#include "stdafx.h"
#include "../Freeze/Freeze.h"
#include "../Snapshots/SnapshotClass.h"
#include "../Snapshots/Snapshots.h"
#include "TracklistFilter.h"
#include "Tracklist.h"

#define RENAME_MSG		0x10005
#define LOADSNAP_MSG	0x10100 // Keep space afterwards

#define MAJORADJUST false
	
// Globals
static SWS_TrackListWnd* g_pList;

// Prototypes
void OpenTrackList(COMMAND_T* = NULL);
void ShowInMCP(COMMAND_T* = NULL);
void ShowInTCP(COMMAND_T* = NULL);
void HideFromMCP(COMMAND_T* = NULL);
void HideFromTCP(COMMAND_T* = NULL);
void TogInTCP(COMMAND_T* = NULL);
void TogInMCP(COMMAND_T* = NULL);
void ShowInMCPOnly(COMMAND_T* = NULL);
void ShowInTCPOnly(COMMAND_T* = NULL);
void ShowInMCPandTCP(COMMAND_T* = NULL);
void HideTracks(COMMAND_T* = NULL);
void ShowSelOnly(COMMAND_T* = NULL);
void ShowInTCPEx(COMMAND_T* = NULL);
void ShowInMCPEx(COMMAND_T* = NULL);
void HideUnSel(COMMAND_T* = NULL);
void ShowAll(COMMAND_T* = NULL);
void NewVisSnapshot(COMMAND_T* = NULL);

enum TL_COLS { COL_NUM, COL_NAME, COL_TCP, COL_MCP, COL_ARM, COL_MUTE, COL_SOLO, /*COL_INPUT, */ NUM_COLS };

static SWS_LVColumn g_cols[] = { { 25, 0, "#" }, { 250, 1, "Name" }, { 40, 2, "TCP" }, { 40, 2, "MCP" },
	{ 40, 2, "Arm", -1 },  { 40, 2, "Mute", -1 }, { 40, 2, "Solo", -1 } /*, { 40, 1, "Input", -1 } */ };

SWS_TrackListView::SWS_TrackListView(HWND hwndList, HWND hwndEdit, SWS_TrackListWnd* pTrackListWnd)
:SWS_ListView(hwndList, hwndEdit, NUM_COLS, g_cols, "TrackList View State", false), m_pTrackListWnd(pTrackListWnd)
{
}

void SWS_TrackListView::GetItemText(LPARAM item, int iCol, char* str, int iStrMax)
{
	MediaTrack* tr = (MediaTrack*)item;
	if (tr)
	{
		switch (iCol)
		{
		case COL_NUM: // #
			_snprintf(str, iStrMax, "%d", CSurf_TrackToID(tr, false));
			break;
		case COL_NAME: // Name
			lstrcpyn(str, (char*)GetSetMediaTrackInfo(tr, "P_NAME", NULL), iStrMax);
			break;
		case COL_TCP: // TCP
			lstrcpyn(str, GetTrackVis(tr) & 2 ? UTF8_BULLET : UTF8_CIRCLE, iStrMax);
			break;
		case COL_MCP: // MCP
			lstrcpyn(str, GetTrackVis(tr) & 1 ? UTF8_BULLET : UTF8_CIRCLE, iStrMax);
			break;
		case COL_ARM:
			lstrcpyn(str, *(int*)GetSetMediaTrackInfo(tr, "I_RECARM", NULL) ? UTF8_BULLET : UTF8_CIRCLE, iStrMax);
			break;
		case COL_MUTE:
			lstrcpyn(str, *(bool*)GetSetMediaTrackInfo(tr, "B_MUTE", NULL) ? UTF8_BULLET : UTF8_CIRCLE, iStrMax);
			break;
		case COL_SOLO:
			lstrcpyn(str, *(int*)GetSetMediaTrackInfo(tr, "I_SOLO", NULL) ? UTF8_BULLET : UTF8_CIRCLE, iStrMax);
			break;
//		case COL_INPUT:
//			_snprintf(str, iStrMax, "%d", *(int*)GetSetMediaTrackInfo(tr, "I_RECINPUT", NULL) + 1);
//			break;
		}
	}
}

void SWS_TrackListView::OnItemClk(LPARAM item, int iCol, int iKeyState)
{
	MediaTrack* tr = (MediaTrack*)item; // Always non-null
	
	if (iCol == COL_TCP || iCol == COL_MCP)
	{
		bool bClickedStar = ((iCol == COL_TCP && GetTrackVis(tr) & 2) || 
							 (iCol == COL_MCP && GetTrackVis(tr) & 1));
		m_bDisableUpdates = true;

		if (m_pTrackListWnd->Linked() && !(iKeyState & LVKF_SHIFT))
		{
			if (iKeyState & LVKF_CONTROL && iKeyState & LVKF_ALT)
				ShowSelOnly();
			else if (bClickedStar)
				HideTracks();
			else
				ShowInMCPandTCP();
		}
		else if (iCol == COL_TCP)
		{
			if (iKeyState & LVKF_CONTROL && iKeyState & LVKF_ALT)
				ShowInTCPEx();
			else if (bClickedStar)
				HideFromTCP();
			else
				ShowInTCP();
		}
		else // iCol == COL_MCP
		{
			if (iKeyState & LVKF_CONTROL && iKeyState & LVKF_ALT)
				ShowInMCPEx();
			else if (bClickedStar)
				HideFromMCP();
			else
				ShowInMCP();
		}
		m_bDisableUpdates = false;
		m_pTrackListWnd->Update();
	}
	else if (iCol == COL_MUTE)
		Main_OnCommand(6, 0);
	else if (iCol == COL_SOLO)
		Main_OnCommand(7, 0);
	else if (iCol == COL_ARM)
		Main_OnCommand(9, 0);
}

void SWS_TrackListView::OnItemDblClk(LPARAM item, int iCol)
{
	Main_OnCommand(40913, 0); // Scroll selected tracks into view
	// TODO new track on NULL?  needs mod of SWS_wnd and all other OnItemDblClk()s
}

void SWS_TrackListView::OnItemSelChanged(LPARAM item, int iState)
{
	MediaTrack* tr = (MediaTrack*)item;
	if (iState & LVIS_FOCUSED)
		g_pList->m_trLastTouched = tr;
	if ((iState & LVIS_SELECTED ? true : false) != (*(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL) ? true : false))
		GetSetMediaTrackInfo(tr, "I_SELECTED", iState & LVIS_SELECTED ? &g_i1 : &g_i0);
}

void SWS_TrackListView::SetItemText(LPARAM item, int iCol, const char* str)
{
	if (iCol == 1)
	{
		MediaTrack* tr = (MediaTrack*)item;
		GetSetMediaTrackInfo(tr, "P_NAME", (char*)str);
	}
}

void SWS_TrackListView::GetItemList(WDL_TypedBuf<LPARAM>* pBuf)
{
	WDL_PtrList<void>* pTracks = m_pTrackListWnd->GetFilter()->Get()->GetFilteredTracks();
	pBuf->Resize(pTracks->GetSize());
	for (int i = 0; i < pTracks->GetSize(); i++)
		pBuf->Get()[i] = (LPARAM)pTracks->Get(i);
}

int SWS_TrackListView::GetItemState(LPARAM item)
{
	MediaTrack* tr = (MediaTrack*)item;
	return *(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL);
}

SWS_TrackListWnd::SWS_TrackListWnd()
:SWS_DockWnd(IDD_TRACKLIST, "Track List", 30003),m_bUpdate(false),
m_trLastTouched(NULL),m_bHideFiltered(false),m_bLink(false),m_cOptionsKey("Track List Options")
{
	// Restore state
	char str[10];
	GetPrivateProfileString(SWS_INI, m_cOptionsKey, "0 0", str, 10, get_ini_file());
	if (strlen(str) == 3)
	{
		m_bHideFiltered = str[0] == '1';
		m_bLink = str[2] == '1';
	}

	if (m_bShowAfterInit)
		Show(false, false);
}

void SWS_TrackListWnd::Update()
{
	static bool bRecurseCheck = false;
	if (!IsValidWindow() || bRecurseCheck || !m_pLists.GetSize() || m_pLists.Get(0)->UpdatesDisabled())
		return;
	bRecurseCheck = true;
	
	//Update the check boxes
	CheckDlgButton(m_hwnd, IDC_HIDE, m_bHideFiltered ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(m_hwnd, IDC_LINK, m_bLink ? BST_CHECKED : BST_UNCHECKED);
	//Update the filter
	char filter[256];
	GetDlgItemText(m_hwnd, IDC_FILTER, filter, 256);
	if (strcmp(filter, m_filter.Get()->GetFilter()))
		SetDlgItemText(m_hwnd, IDC_FILTER, m_filter.Get()->GetFilter());

	m_filter.Get()->GetFilteredTracks();
	m_filter.Get()->UpdateReaper(m_bHideFiltered);

	m_pLists.Get(0)->Update();

	bRecurseCheck = false;
}

void SWS_TrackListWnd::ClearFilter()
{
	if (IsValidWindow())
		SendMessage(m_hwnd, WM_COMMAND, IDC_CLEAR, 0);
	else
	{
		m_filter.Get()->SetFilter("");
		m_filter.Get()->UpdateReaper(m_bHideFiltered);
	}
}

void SWS_TrackListWnd::OnInitDlg()
{
	m_resize.init_item(IDC_LIST, 0.0, 0.0, 1.0, 1.0);
	m_resize.init_item(IDC_STATIC_FILTER, 0.0, 1.0, 0.0, 1.0);
	m_resize.init_item(IDC_FILTER, 0.0, 1.0, 1.0, 1.0);
	m_resize.init_item(IDC_CLEAR, 0.0, 1.0, 0.0, 1.0);
	m_resize.init_item(IDC_HIDE, 0.0, 1.0, 0.0, 1.0);
	m_resize.init_item(IDC_LINK, 0.0, 1.0, 0.0, 1.0);

	m_pLists.Add(new SWS_TrackListView(GetDlgItem(m_hwnd, IDC_LIST), GetDlgItem(m_hwnd, IDC_EDIT), this));
	SetWindowLongPtr(GetDlgItem(m_hwnd, IDC_FILTER), GWLP_USERDATA, 0xdeadf00b);

	Update();

	SetTimer(m_hwnd, 0, 100, NULL);
}

void SWS_TrackListWnd::OnCommand(WPARAM wParam, LPARAM lParam)
{
	switch (wParam)
	{
		case IDC_CLEAR | (BN_CLICKED << 16):
			SetWindowText(GetDlgItem(m_hwnd, IDC_FILTER), "");
			break;
		case IDC_FILTER | (EN_CHANGE << 16):
		{
			char curFilter[256];
			GetWindowText(GetDlgItem(m_hwnd, IDC_FILTER), curFilter, 256);
			m_filter.Get()->SetFilter(curFilter);
			Update();
			break;
		}
		case IDC_HIDE:
			m_bHideFiltered = IsDlgButtonChecked(m_hwnd, IDC_HIDE) == BST_CHECKED ? true : false;
			Update();
			break;
		case IDC_LINK:
			m_bLink = IsDlgButtonChecked(m_hwnd, IDC_LINK) == BST_CHECKED ? true : false;
			Update();
			break;
		case RENAME_MSG:
			if (m_trLastTouched)
				m_pLists.Get(0)->EditListItem((LPARAM)m_trLastTouched, 1);
			break;
		default:
			if (wParam >= LOADSNAP_MSG)
				GetSnapshot((int)(wParam - LOADSNAP_MSG), ALL_MASK, false);
			else
				Main_OnCommand((int)wParam, (int)lParam);
	}
}

HMENU SWS_TrackListWnd::OnContextMenu(int x, int y)
{
	HMENU contextMenu = CreatePopupMenu();

	AddToMenu(contextMenu, "Snapshot current track visibility", SWSGetCommandID(NewVisSnapshot));
	Snapshot* s;
	int i = 0;
	while((s = GetSnapshotPtr(i++)) != NULL)
	{
		if (s->m_iMask == VIS_MASK)
		{
			char cMenu[50];
			int iCmd = SWSGetCommandID(GetSnapshot, s->m_iSlot);
			if (!iCmd)
				iCmd = LOADSNAP_MSG + s->m_iSlot;
			_snprintf(cMenu, 50, "Recall snapshot %s", s->m_cName);
			AddToMenu(contextMenu, cMenu, iCmd);
		}
	}

	AddToMenu(contextMenu, "Show all tracks", SWSGetCommandID(ShowAll));
	AddToMenu(contextMenu, "Show SWS Snapshots", SWSGetCommandID(OpenSnapshotsDialog));

	LPARAM item = m_pLists.Get(0)->GetHitItem(x, y, NULL);
	if (item)
	{
		m_trLastTouched = (MediaTrack*)item;
		AddToMenu(contextMenu, SWS_SEPARATOR, 0);
		AddToMenu(contextMenu, "Rename", RENAME_MSG);
		AddToMenu(contextMenu, SWS_SEPARATOR, 0);
		AddToMenu(contextMenu, "Show only in MCP", SWSGetCommandID(ShowInMCPOnly));
		AddToMenu(contextMenu, "Show only in TCP", SWSGetCommandID(ShowInTCPOnly));
		AddToMenu(contextMenu, "Show in both MCP and TCP", SWSGetCommandID(ShowInMCPandTCP));
		AddToMenu(contextMenu, "Hide in both MCP and TCP", SWSGetCommandID(HideTracks));
		AddToMenu(contextMenu, SWS_SEPARATOR, 0);
		AddToMenu(contextMenu, "Invert selection", SWSGetCommandID(TogTrackSel));
		AddToMenu(contextMenu, "Hide unselected", SWSGetCommandID(HideUnSel));

		// Check current state
		switch(GetTrackVis(m_trLastTouched))
		{
			case 0: CheckMenuItem(contextMenu, SWSGetCommandID(HideTracks),      MF_BYCOMMAND | MF_CHECKED); break;
			case 1: CheckMenuItem(contextMenu, SWSGetCommandID(ShowInMCPOnly),   MF_BYCOMMAND | MF_CHECKED); break;
			case 2: CheckMenuItem(contextMenu, SWSGetCommandID(ShowInTCPOnly),   MF_BYCOMMAND | MF_CHECKED); break;
			case 3: CheckMenuItem(contextMenu, SWSGetCommandID(ShowInMCPandTCP), MF_BYCOMMAND | MF_CHECKED); break;
		}
	}

	return contextMenu;
}

void SWS_TrackListWnd::OnDestroy()
{
	m_bUpdate = false;

	KillTimer(m_hwnd, 0);

	char str[10];
	_snprintf(str, 10, "%d %d", m_bHideFiltered ? 1 : 0, m_bLink ? 1 : 0);
	WritePrivateProfileString(SWS_INI, m_cOptionsKey, str, get_ini_file());
}

void SWS_TrackListWnd::OnTimer()
{
	if (m_bUpdate)
	{
		Update();
		m_bUpdate = false;
	}
}

int SWS_TrackListWnd::OnKey(MSG* msg, int iKeyState)
{
	if (msg->message == WM_KEYDOWN)
	{
		if (!iKeyState) switch (msg->wParam)
		{
		case VK_LEFT:
			TogInTCP();
			return 1;
		case VK_RIGHT:
			TogInMCP();
			return 1;
		case VK_DELETE:
			Main_OnCommand(40005, 0); // remove selected tracks
			return 1;
		case VK_F2:
			OnCommand(RENAME_MSG, 0);
			return 1;
		}

		// For some "odd" reason, shift-up and shift-down don't work on the tracklist,
		// so we handle them here with this obtuse code.  There's perhaps a better way, but I don't
		// know what it is.
		if (iKeyState == LVKF_SHIFT && (msg->wParam == VK_DOWN || msg->wParam == VK_UP))
		{
			int iTouched = m_trLastTouched ? CSurf_TrackToID(m_trLastTouched, false) : -1;
			if (!GetNumTracks() || (msg->wParam == VK_DOWN && iTouched == GetNumTracks()) || (msg->wParam == VK_UP && iTouched == 1))
				return 1;

			// Find the first and last selected track
			int iFirst = 0, iLast = 0;
			for (int i = 1; i <= GetNumTracks(); i++)
				if (*(int*)GetSetMediaTrackInfo(CSurf_TrackFromID(i, false), "I_SELECTED", NULL))
				{
					if (!iFirst)
						iFirst = i;
					iLast = i;
				}

			if (iTouched == -1)
				iTouched = msg->wParam == VK_DOWN ? iLast : iFirst;
			else
			{
				// Find the focused track, and un-focus it
				for (int i = 0; i < m_pLists.Get(0)->GetListItemCount(); i++)
					if (m_pLists.Get(0)->GetListItem(i) == (LPARAM)m_trLastTouched)
					{
						ListView_SetItemState(m_pLists.Get(0)->GetHWND(), i, 0, LVIS_FOCUSED);
						break;
					}
			}

			m_pLists.Get(0)->DisableUpdates(true);

			if (!iFirst)
			{	// Nothing selected, so select last touched, or the first/last track
				if (!m_trLastTouched)
					m_trLastTouched = CSurf_TrackFromID(msg->wParam == VK_DOWN ? 1 : GetNumTracks(), false);
				GetSetMediaTrackInfo(m_trLastTouched, "I_SELECTED", &g_i1);
			}
			else if (msg->wParam == VK_DOWN)
			{
				if (iTouched > iFirst || iFirst == iLast)
				{
					m_trLastTouched = CSurf_TrackFromID(iTouched+1, false);
					GetSetMediaTrackInfo(m_trLastTouched, "I_SELECTED", &g_i1);
				}
				else if (iTouched == iFirst)
				{
					GetSetMediaTrackInfo(m_trLastTouched, "I_SELECTED", &g_i0);
					m_trLastTouched = CSurf_TrackFromID(iFirst+1, false);
				}
			}
			else // VK_UP
			{
				if (iTouched < iLast || iFirst == iLast)
				{
					m_trLastTouched = CSurf_TrackFromID(iTouched-1, false);
					GetSetMediaTrackInfo(m_trLastTouched, "I_SELECTED", &g_i1);
				}
				else if (iTouched == iLast)
				{
					GetSetMediaTrackInfo(m_trLastTouched, "I_SELECTED", &g_i0);
					m_trLastTouched = CSurf_TrackFromID(iLast-1, false);
				}
			}

			m_pLists.Get(0)->DisableUpdates(false);
			Update();
			
			// Update the focus
			for (int i = 0; i < m_pLists.Get(0)->GetListItemCount(); i++)
				if (m_pLists.Get(0)->GetListItem(i) == (LPARAM)m_trLastTouched)
				{
					ListView_SetItemState(m_pLists.Get(0)->GetHWND(), i, LVIS_FOCUSED, LVIS_FOCUSED);
					break;
				}
			return 1;
		}
		else if (iKeyState == LVKF_SHIFT && msg->wParam == VK_UP)
		{
			return 1;
		}
	}
	return 0;
}

void ScheduleTracklistUpdate()
{
	if (g_pList)
		g_pList->ScheduleUpdate();
}

void OpenTrackList(COMMAND_T*)
{
	g_pList->Show(true, true);
}

void OpenTrackListFilt(COMMAND_T* = NULL)
{
	g_pList->Show(false, true);
	HWND hwnd = g_pList->GetHWND();
	SetFocus(GetDlgItem(hwnd, IDC_FILTER));
	SendMessage(GetDlgItem(hwnd, IDC_FILTER), EM_SETSEL, 0, -1);
}

void ShowInMCPOnly(COMMAND_T*)
{
	for (int i = 1; i <= GetNumTracks(); i++)
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		if (*(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
			SetTrackVis(tr, 1);
	}
	TrackList_AdjustWindows(MAJORADJUST);
	UpdateTimeline();
	Undo_OnStateChangeEx("Show selected track(s) in MCP only", UNDO_STATE_TRACKCFG, -1);
}

void ShowInTCPOnly(COMMAND_T*)
{
	for (int i = 1; i <= GetNumTracks(); i++)
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		if (*(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
			SetTrackVis(tr, 2);
	}
	TrackList_AdjustWindows(MAJORADJUST);
	UpdateTimeline();
	Undo_OnStateChangeEx("Show selected track(s) in TCP only", UNDO_STATE_TRACKCFG, -1);
}

void ShowInMCPandTCP(COMMAND_T*)
{
	for (int i = 1; i <= GetNumTracks(); i++)
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		if (*(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
			SetTrackVis(tr, 3);
	}
	TrackList_AdjustWindows(MAJORADJUST);
	UpdateTimeline();
	Undo_OnStateChangeEx("Show selected track(s) in TCP and MCP", UNDO_STATE_TRACKCFG, -1);
}

void HideTracks(COMMAND_T*)
{
	for (int i = 1; i <= GetNumTracks(); i++)
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		if (*(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
			SetTrackVis(tr, 0);
	}
	TrackList_AdjustWindows(MAJORADJUST);
	UpdateTimeline();
	Undo_OnStateChangeEx("Hide selected track(s)", UNDO_STATE_TRACKCFG, -1);
}

void ShowInMCP(COMMAND_T*)
{
	for (int i = 1; i <= GetNumTracks(); i++)
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		if (*(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
			SetTrackVis(tr, GetTrackVis(tr) | 1);
	}
	TrackList_AdjustWindows(MAJORADJUST);
	UpdateTimeline();
	Undo_OnStateChangeEx("Show selected track(s) in MCP", UNDO_STATE_TRACKCFG, -1);
}

void ShowInTCP(COMMAND_T*)
{
	for (int i = 1; i <= GetNumTracks(); i++)
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		if (*(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
			SetTrackVis(tr, GetTrackVis(tr) | 2);
	}
	TrackList_AdjustWindows(MAJORADJUST);
	UpdateTimeline();
	Undo_OnStateChangeEx("Show selected track(s) in TCP", UNDO_STATE_TRACKCFG, -1);
}

void HideFromMCP(COMMAND_T*)
{
	for (int i = 1; i <= GetNumTracks(); i++)
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		if (*(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
			SetTrackVis(tr, GetTrackVis(tr) & ~1);
	}
	TrackList_AdjustWindows(MAJORADJUST);
	UpdateTimeline();
	Undo_OnStateChangeEx("Hide selected track(s) from MCP", UNDO_STATE_TRACKCFG, -1);
}

void HideFromTCP(COMMAND_T*)
{
	for (int i = 1; i <= GetNumTracks(); i++)
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		if (*(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
			SetTrackVis(tr, GetTrackVis(tr) & ~2);
	}
	TrackList_AdjustWindows(MAJORADJUST);
	UpdateTimeline();
	Undo_OnStateChangeEx("Hide selected track(s) from TCP", UNDO_STATE_TRACKCFG, -1);
}

void TogInMCP(COMMAND_T*)
{
	for (int i = 1; i <= GetNumTracks(); i++)
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		if (*(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
			SetTrackVis(tr, GetTrackVis(tr) ^ 1);
	}
	TrackList_AdjustWindows(MAJORADJUST);
	UpdateTimeline();
	Undo_OnStateChangeEx("Toggle selected track(s) visible in MCP", UNDO_STATE_TRACKCFG, -1);
}

void TogInTCP(COMMAND_T*)
{
	for (int i = 1; i <= GetNumTracks(); i++)
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		if (*(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
			SetTrackVis(tr, GetTrackVis(tr) ^ 2);
	}
	TrackList_AdjustWindows(MAJORADJUST);
	UpdateTimeline();
	Undo_OnStateChangeEx("Toggle selected track(s) visible in TCP", UNDO_STATE_TRACKCFG, -1);
}

void ToggleHide(COMMAND_T*)
{
	for (int i = 1; i <= GetNumTracks(); i++)
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		if (*(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
			SetTrackVis(tr, GetTrackVis(tr) ? 0 : 3);
	}
	TrackList_AdjustWindows(MAJORADJUST);
	UpdateTimeline();
	Undo_OnStateChangeEx("Toggle selected track(s) fully visible/hidden", UNDO_STATE_TRACKCFG, -1);
}

void ShowAll(COMMAND_T*)
{
	for (int i = 1; i <= GetNumTracks(); i++)
		SetTrackVis(CSurf_TrackFromID(i, false), 3);
	TrackList_AdjustWindows(MAJORADJUST);
	UpdateTimeline();
	Undo_OnStateChangeEx("Show all tracks", UNDO_STATE_TRACKCFG, -1);
}

void ShowAllMCP(COMMAND_T*)
{
	for (int i = 1; i <= GetNumTracks(); i++)
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		SetTrackVis(tr, GetTrackVis(tr) | 1);
	}
	TrackList_AdjustWindows(MAJORADJUST);
	UpdateTimeline();
	Undo_OnStateChangeEx("Show all tracks in MCP", UNDO_STATE_TRACKCFG, -1);
}

void ShowAllTCP(COMMAND_T*)
{
	for (int i = 1; i <= GetNumTracks(); i++)
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		SetTrackVis(tr, GetTrackVis(tr) | 2);
	}
	TrackList_AdjustWindows(MAJORADJUST);
	UpdateTimeline();
	Undo_OnStateChangeEx("Show all tracks in TCP", UNDO_STATE_TRACKCFG, -1);
}

void HideAll(COMMAND_T* = NULL)
{
	for (int i = 1; i <= GetNumTracks(); i++)
		SetTrackVis(CSurf_TrackFromID(i, false), 0);
	TrackList_AdjustWindows(MAJORADJUST);
	UpdateTimeline();
	Undo_OnStateChangeEx("Hide all tracks", UNDO_STATE_TRACKCFG, -1);
}

void ShowInMCPEx(COMMAND_T*)
{
	for (int i = 1; i <= GetNumTracks(); i++)
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		int iVis = GetTrackVis(tr);
		if (*(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
			SetTrackVis(tr, iVis | 1);
		else
			SetTrackVis(tr, iVis & 2);

	}
	TrackList_AdjustWindows(MAJORADJUST);
	UpdateTimeline();
	Undo_OnStateChangeEx("Show selected track(s) in MCP, hide others", UNDO_STATE_TRACKCFG, -1);
}

void ShowInTCPEx(COMMAND_T*)
{
	for (int i = 1; i <= GetNumTracks(); i++)
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		int iVis = GetTrackVis(tr);
		if (*(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
			SetTrackVis(tr, iVis | 2);
		else
			SetTrackVis(tr, iVis & 1);

	}
	TrackList_AdjustWindows(MAJORADJUST);
	UpdateTimeline();
	Undo_OnStateChangeEx("Show selected track(s) in TCP, hide others", UNDO_STATE_TRACKCFG, -1);
}

void ShowSelOnly(COMMAND_T*)
{
	for (int i = 1; i <= GetNumTracks(); i++)
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		if (*(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
			SetTrackVis(tr, 3);
		else
			SetTrackVis(tr, 0);

	}
	TrackList_AdjustWindows(MAJORADJUST);
	UpdateTimeline();
	Undo_OnStateChangeEx("Show selected track(s), hide others", UNDO_STATE_TRACKCFG, -1);
}

void HideUnSel(COMMAND_T*)
{
	for (int i = 1; i <= GetNumTracks(); i++)
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		if (!*(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
			SetTrackVis(tr, 0);
	}
	TrackList_AdjustWindows(MAJORADJUST);
	UpdateTimeline();
	Undo_OnStateChangeEx("Hide unselected track(s)", UNDO_STATE_TRACKCFG, -1);
}

static void ClearFilter(COMMAND_T*)
{
	g_pList->ClearFilter();
}

void NewVisSnapshot(COMMAND_T*)
{
	NewSnapshot(VIS_MASK, false);
}

static bool ProcessExtensionLine(const char *line, ProjectStateContext *ctx, bool isUndo, struct project_config_extension_t *reg)
{
	LineParser lp(false);
	if (lp.parse(line) || lp.getnumtokens() < 1)
		return false;
	if (strcmp(lp.gettoken_str(0), "<SWSTRACKFILTER") == 0)
	{
		g_pList->GetFilter()->Get()->SetFilter(lp.gettoken_str(1));
		char linebuf[4096];
		while(true)
		{
			if (!ctx->GetLine(linebuf,sizeof(linebuf)) && !lp.parse(linebuf))
			{
				if (lp.gettoken_str(0)[0] == '>')
					break;
				g_pList->GetFilter()->Get()->Init(&lp);
			}
			else
				break;
		}
		return true;
	}
	return false;
}

static void SaveExtensionConfig(ProjectStateContext *ctx, bool isUndo, struct project_config_extension_t *reg)
{
	char str[4096];
	bool bDone;
	while (g_pList->GetFilter()->Get()->ItemString(str, 4096, &bDone))
	{
		ctx->AddLine(str);
		if (bDone)
			break;
	}
}

static void BeginLoadProjectState(bool isUndo, struct project_config_extension_t *reg)
{
	g_pList->GetFilter()->Get()->SetFilter(NULL);
	g_pList->GetFilter()->Cleanup();
}

static bool TrackListWindowEnabled(COMMAND_T*)
{
	return g_pList->IsValidWindow();
}

static project_config_extension_t g_projectconfig = { ProcessExtensionLine, SaveExtensionConfig, BeginLoadProjectState, NULL };

static COMMAND_T g_commandTable[] =
{
	{ { DEFACCEL, "SWS: Show Tracklist" },								"SWSTL_OPEN",		OpenTrackList,		"SWS Tracklist", 0, TrackListWindowEnabled },
	{ { DEFACCEL, "SWS: Show Tracklist with filter focused" },			"SWSTL_OPENFILT",	OpenTrackListFilt,	NULL, },

	// Set all bits
	{ { DEFACCEL, "SWS: Show selected track(s) in MCP only" },			"SWSTL_MCPONLY",	ShowInMCPOnly,		NULL, },
	{ { DEFACCEL, "SWS: Show selected track(s) in TCP only" },			"SWSTL_TCPONLY",	ShowInTCPOnly,		NULL, },
	{ { DEFACCEL, "SWS: Show selected track(s) in TCP and MCP" },		"SWSTL_BOTH",		ShowInMCPandTCP,	NULL, },
	{ { DEFACCEL, "SWS: Hide selected track(s)" },						"SWSTL_HIDE",		HideTracks,			NULL, },

	// Set bits individually
	{ { DEFACCEL, "SWS: Show selected track(s) in MCP" },				"SWSTL_SHOWMCP",	ShowInMCP,			NULL, },
	{ { DEFACCEL, "SWS: Show selected track(s) in TCP" },				"SWSTL_SHOWTCP",	ShowInTCP,			NULL, },
	{ { DEFACCEL, "SWS: Hide selected track(s) from MCP" },				"SWSTL_HIDEMCP",	HideFromMCP,		NULL, },
	{ { DEFACCEL, "SWS: Hide selected track(s) from TCP" },				"SWSTL_HIDETCP",	HideFromTCP,		NULL, },

	// Toggle
	{ { DEFACCEL, "SWS: Toggle selected track(s) visible in MCP" },		"SWSTL_TOGMCP",		TogInMCP,			NULL, },
	{ { DEFACCEL, "SWS: Toggle selected track(s) visible in TCP" },		"SWSTL_TOGTCP",		TogInTCP,			NULL, },
	{ { DEFACCEL, "SWS: Toggle selected track(s) fully visible/hidden" }, "SWSTL_TOGGLE",	ToggleHide,			NULL, },

	// Affect all tracks
	{ { DEFACCEL, "SWS: Show all tracks" },								"SWSTL_SHOWALL",	ShowAll,			NULL, },
	{ { DEFACCEL, "SWS: Show all tracks in MCP" },						"SWSTL_SHOWALLMCP",	ShowAllMCP,			NULL, },
	{ { DEFACCEL, "SWS: Show all tracks in TCP" },						"SWSTL_SHOWALLTCP",	ShowAllTCP,			NULL, },
	{ { DEFACCEL, "SWS: Hide all tracks" },								"SWSTL_HIDEALL",	HideAll,			NULL, },
	{ { DEFACCEL, "SWS: Show selected track(s) in MCP, hide others" },	"SWSTL_SHOWMCPEX",	ShowInMCPEx,		NULL, },
	{ { DEFACCEL, "SWS: Show selected track(s) in TCP, hide others" },	"SWSTL_SHOWTCPEX",	ShowInTCPEx,		NULL, },
	{ { DEFACCEL, "SWS: Show selected track(s), hide others" },			"SWSTL_SHOWEX",		ShowSelOnly,		NULL, },
	{ { DEFACCEL, "SWS: Hide unselected track(s)" },					"SWSTL_HIDEUNSEL",	HideUnSel,			NULL, },

	{ { DEFACCEL, "SWS: Clear tracklist filter" },                       "SWSTL_CLEARFLT",   ClearFilter,		NULL, },
	{ { DEFACCEL, "SWS: Snapshot current track visibility" },            "SWSTL_SNAPSHOT",   NewVisSnapshot,		NULL, },
	{ {}, LAST_COMMAND, }, // Denote end of table
};

static void menuhook(const char* menustr, HMENU hMenu, int flag)
{
	if (strcmp(menustr, "Main view") == 0 && flag == 0)
		AddToMenu(hMenu, g_commandTable[0].menuText, g_commandTable[0].accel.accel.cmd, 40075);
}

int TrackListInit()
{
	if (!plugin_register("projectconfig",&g_projectconfig))
		return 0;

	SWSRegisterCommands(g_commandTable);

	if (!plugin_register("hookcustommenu", (void*)menuhook))
		return 0;

	g_pList = new SWS_TrackListWnd;

	return 1;
}

void TrackListExit()
{
	delete g_pList;
}