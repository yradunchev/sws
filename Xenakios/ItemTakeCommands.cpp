/******************************************************************************
/ ItemTakeCommands.cpp
/
/ Copyright (c) 2009 Tim Payne (SWS), original code by Xenakios
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
#include "Parameters.h"

using namespace std;

int XenGetProjectItems(vector<MediaItem*>& TheItems,bool OnlySelectedItems, bool IncEmptyItems)
{
	TheItems.clear();
	for (int i = 0; i < GetNumTracks(); i++)
	{
		MediaTrack* CurTrack = CSurf_TrackFromID(i+1, false);
		for (int j = 0; j < GetTrackNumMediaItems(CurTrack); j++)
		{
			MediaItem* CurItem = GetTrackMediaItem(CurTrack, j);
			if (CurItem && (!OnlySelectedItems || *(bool*)GetSetMediaItemInfo(CurItem, "B_UISEL", NULL)))
			{
				if (IncEmptyItems || GetMediaItemNumTakes(CurItem) > 0)
					TheItems.push_back(CurItem);
			}
		}
	}
	return (int)TheItems.size();
}

int XenGetProjectTakes(vector<MediaItem_Take*>& TheTakes, bool OnlyActive, bool OnlyFromSelectedItems)
{
	TheTakes.clear();
	for (int i = 0; i < GetNumTracks(); i++)
	{
		MediaTrack* CurTrack = CSurf_TrackFromID(i+1,false);
		for (int j = 0; j < GetTrackNumMediaItems(CurTrack); j++)
		{
			MediaItem* CurItem=GetTrackMediaItem(CurTrack,j);
			if (CurItem && (!OnlyFromSelectedItems || *(bool*)GetSetMediaItemInfo(CurItem,"B_UISEL",NULL)))
			{
				if (OnlyActive)
				{
					MediaItem_Take* CurTake = GetMediaItemTake(CurItem, -1);
					if (CurTake)
						TheTakes.push_back(CurTake);
				}
				else for (int k = 0; k < GetMediaItemNumTakes(CurItem); k++)
				{
					MediaItem_Take* CurTake = GetMediaItemTake(CurItem, k);
					if (CurTake)
						TheTakes.push_back(CurTake);
				}
			}
		}
	}
	return (int)TheTakes.size();
}

void DoMoveItemsLeftByItemLen(COMMAND_T*)
{
	vector<MediaItem*> VecItems;
	for (int i = 0; i < GetNumTracks(); i++)
	{
		MediaTrack* CurTrack=CSurf_TrackFromID(i+1,false);
		for (int j = 0; j < GetTrackNumMediaItems(CurTrack); j++)
		{
			MediaItem* CurItem = GetTrackMediaItem(CurTrack,j);
			bool isSel = *(bool*)GetSetMediaItemInfo(CurItem, "B_UISEL", NULL);
			if (isSel) VecItems.push_back(CurItem);
		}
	}
	for (int i = 0; i < (int)VecItems.size(); i++)
	{
		double CurPos = *(double*)GetSetMediaItemInfo(VecItems[i], "D_POSITION", NULL);
		double dItemLen = *(double*)GetSetMediaItemInfo(VecItems[i], "D_LENGTH", NULL);
		double NewPos = CurPos - dItemLen;
		GetSetMediaItemInfo(VecItems[i], "D_POSITION", &NewPos);
	}
	UpdateTimeline();
	Undo_OnStateChangeEx("Move item back by item length", 4, -1);
}

void DoToggleTakesNormalize(COMMAND_T*)
{
	//
	WDL_PtrList<MediaItem_Take> *TheTakes=new (WDL_PtrList<MediaItem_Take>);
	int NumActiveTakes=GetActiveTakes(TheTakes);
	int NumNormalizedTakes=0;
	for (int i=0;i<NumActiveTakes;i++)
	{
		//
		double TakeVol=*(double*)GetSetMediaItemTakeInfo(TheTakes->Get(i),"D_VOL",NULL);
		if (TakeVol!=1.0) NumNormalizedTakes++;
	}
	if (NumNormalizedTakes>0)
	{
		for (int i=0;i<NumActiveTakes;i++)
		{
			//
			double TheGain=1.0;
			GetSetMediaItemTakeInfo(TheTakes->Get(i),"D_VOL",&TheGain);
		}
		Undo_OnStateChangeEx("Set take(s) to unity gain",4,-1);
		UpdateTimeline();

	}
	else
	{
		Main_OnCommand(40108, 0);
		Undo_OnStateChangeEx("Normalize take(s)",4,-1);
		UpdateTimeline();
	}

	delete TheTakes;
		
}


void DoSetVolPan(double Vol, double Pan, bool SetVol, bool SetPan)
{
	//
	vector<MediaItem_Take*> TheTakes;
	XenGetProjectTakes(TheTakes,true,true);
	double NewVol=Vol;
	double NewPan=Pan;
	for (int i=0;i<(int)TheTakes.size();i++)
	{
		if (SetVol) GetSetMediaItemTakeInfo(TheTakes[i],"D_VOL",&NewVol);
		if (SetPan) GetSetMediaItemTakeInfo(TheTakes[i],"D_PAN",&NewPan);

	}
	Undo_OnStateChangeEx("Set take volume and pan",4,-1);
	UpdateTimeline();
}

void DoSetItemVols(double theVol)
{
	for (int i=0;i<GetNumTracks();i++)
	{
		MediaTrack* pTrack = CSurf_TrackFromID(i+1, false);
		for (int j = 0; j < GetTrackNumMediaItems(pTrack); j++)
		{
			MediaItem* CurItem = GetTrackMediaItem(pTrack,j);
			if (*(bool*)GetSetMediaItemInfo(CurItem, "B_UISEL", NULL))
				GetSetMediaItemInfo(CurItem, "D_VOL", &theVol);
		}
	}
	Undo_OnStateChangeEx("Set item volume", 4, -1);
	UpdateTimeline();
}


WDL_DLGRET ItemSetVolDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch(Message)
    {
        case WM_INITDIALOG:
		{	
			SetDlgItemText(hwnd, IDC_VOLEDIT, "0.0");
			SetFocus(GetDlgItem(hwnd, IDC_VOLEDIT));
			SendMessage(GetDlgItem(hwnd, IDC_VOLEDIT), EM_SETSEL, 0, -1);
			break;
		}
        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDOK:
				{
					char tbuf[100];
					GetDlgItemText(hwnd,IDC_VOLEDIT,tbuf,99);
					double NewVol=1.0;
					bool WillSetVolume=false;
					if (strcmp(tbuf,"")!=0)
					{
						WillSetVolume=true;
						NewVol=atof(tbuf);
						if (NewVol>-144.0)
							NewVol=exp(NewVol*0.115129254);
						else NewVol=0.0;

					}
					if (WillSetVolume)
						DoSetItemVols(NewVol);
					
					EndDialog(hwnd,0);
					break;
				}
				case IDCANCEL:
					EndDialog(hwnd,0);
					break;
			}
	}
	return 0;
}	

void DoShowItemVolumeDialog(COMMAND_T*)
{
	DialogBox(g_hInst, MAKEINTRESOURCE(IDD_ITEMVOLUME), g_hwndParent, ItemSetVolDlgProc);
}

WDL_DLGRET ItemPanVolDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	//
	switch(Message)
    {
        case WM_INITDIALOG:
			
			SetDlgItemText(hwnd, IDC_VOLEDIT, "");
			SetDlgItemText(hwnd, IDC_PANEDIT, "");
			SetFocus(GetDlgItem(hwnd, IDC_VOLEDIT));
			SendMessage(GetDlgItem(hwnd, IDC_VOLEDIT), EM_SETSEL, 0, -1);
			break;
        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDOK:
				{
					char TempString[100];
					GetDlgItemText(hwnd,IDC_VOLEDIT,TempString,99);
					bool SetVol=FALSE;
					bool SetPan=FALSE;
					double NewVol=strtod(TempString,NULL);
					
					if (strcmp(TempString,"")!=0)
					{
						if (NewVol>-144.0)
							NewVol=exp(NewVol*0.115129254);
						else
							NewVol=0;
						SetVol=TRUE;
					} 
					
					GetDlgItemText (hwnd,IDC_PANEDIT,TempString,99);
					double NewPan=strtod(TempString,NULL);
					if (strcmp(TempString,"")!=0) 
						SetPan=TRUE;
					DoSetVolPan(NewVol,NewPan/100.0,SetVol,SetPan);
					EndDialog(hwnd,0);
					break;
				}
				case IDCANCEL:
					EndDialog(hwnd,0);
					break;
			}
	}
	return 0;
}

void PasteMultipletimes(int NumPastes, double TimeIntervalQN, int RepeatMode)
{
	if (NumPastes > 0)
	{
		int* pCursorMode = (int*)GetConfigVar("itemclickmovecurs");
		double dStartTime = GetCursorPosition();

		Undo_BeginBlock();

		switch (RepeatMode)
		{
			case 0: // No gaps
			{
				// Cursor mode must set to move the cursor after paste
				// Clear bit 8 of itemclickmovecurs
				int savedMode = *pCursorMode;
				*pCursorMode &= ~8;
				for (int i = 0; i < NumPastes; i++)
					Main_OnCommand(40058,0); // Paste
				*pCursorMode = savedMode; // Restore the cursor mode
				break;
			}
			case 1: // Time interval
				for (int i = 0; i < NumPastes; i++)
				{
					Main_OnCommand(40058,0); // Paste
					SetEditCurPos(dStartTime + (i+1) * TimeIntervalQN, false, false);
				}
				break;
			case 2: // Beat interval
			{
				double dStartBeat = TimeMap_timeToQN(dStartTime);
				for (int i = 0; i < NumPastes; i++)
				{
					Main_OnCommand(40058,0); // Paste
					SetEditCurPos(TimeMap_QNToTime(dStartBeat + (i+1) * TimeIntervalQN), false, false);
				}
				break;
			}
		}
		if (*pCursorMode & 8) // If "don't move cursor after paste" move the cursor back
			SetEditCurPos(dStartTime, false, false);
			
		Undo_EndBlock("Repeat Paste",0);
	}
}



WDL_DLGRET RepeatPasteDialogProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	static double dTimeInterval = 1.0;
	static double dBeatInterval = 1.0;
	static char cBeatStr[50] = "1";
	static int iNumRepeats = 1;
	static int iRepeatMode = 2;	

	switch(Message)
    {
		case WM_INITDIALOG:
		{
			char TextBuf[32];
			sprintf(TextBuf,"%.2f", dTimeInterval);
			SetDlgItemText(hwnd, IDC_EDIT1, TextBuf);

			sprintf(TextBuf,"%d", iNumRepeats);
			SetDlgItemText(hwnd, IDC_NUMREPEDIT, TextBuf);
			InitFracBox(GetDlgItem(hwnd, IDC_NOTEVALUECOMBO), cBeatStr);

			SetFocus(GetDlgItem(hwnd, IDC_NUMREPEDIT));
			SendMessage(GetDlgItem(hwnd, IDC_NUMREPEDIT), EM_SETSEL, 0, -1);

			switch (iRepeatMode)
			{
				case 0: CheckDlgButton(hwnd, IDC_RADIO1, BST_CHECKED); break;
				case 1: CheckDlgButton(hwnd, IDC_RADIO2, BST_CHECKED); break;
				case 2: CheckDlgButton(hwnd, IDC_RADIO3, BST_CHECKED); break;
			}
			break;
		}
		case WM_COMMAND:
            switch(LOWORD(wParam))
            {
				case IDOK:
				{
					char str[100];
					GetDlgItemText(hwnd, IDC_NUMREPEDIT, str, 100);
					iNumRepeats = atoi(str);
					
					if (IsDlgButtonChecked(hwnd, IDC_RADIO1) == BST_CHECKED)
						iRepeatMode = 0;
					else if (IsDlgButtonChecked(hwnd, IDC_RADIO2) == BST_CHECKED)
						iRepeatMode = 1;
					else if (IsDlgButtonChecked(hwnd, IDC_RADIO3) == BST_CHECKED)
						iRepeatMode = 2;

					if (iRepeatMode == 2)
					{
						GetDlgItemText(hwnd, IDC_NOTEVALUECOMBO, cBeatStr, 100);
						dBeatInterval = parseFrac(cBeatStr);
						PasteMultipletimes(iNumRepeats, dBeatInterval, iRepeatMode);
					}
					else if (iRepeatMode == 1)
					{
						GetDlgItemText(hwnd, IDC_EDIT1, str, 100);
						dTimeInterval = atof(str);
						PasteMultipletimes(iNumRepeats, dTimeInterval, iRepeatMode);
					}
					else if (iRepeatMode == 0)
						PasteMultipletimes(iNumRepeats, 1.0, iRepeatMode);

					EndDialog(hwnd,0);
					break;
				}
				case IDCANCEL:
					EndDialog(hwnd,0);
					break;
		}
	}
	return 0;
}


void DoShowVolPanDialog(COMMAND_T*)
{
	DialogBox(g_hInst, MAKEINTRESOURCE(IDD_ITEMPANVOLDIALOG), g_hwndParent, ItemPanVolDlgProc);
}

void DoChooseNewSourceFileForSelTakes(COMMAND_T*)
{
	WDL_PtrList<MediaItem_Take> *TheTakes=new (WDL_PtrList<MediaItem_Take>);
	PCM_source *ThePCMSource;
	int i;
	int NumActiveTakes=GetActiveTakes(TheTakes);
	if (NumActiveTakes>0)
	{
		MediaItem_Take* CurTake;
		char* cFileName = BrowseForFiles("Choose new source file", NULL, NULL, false, plugin_getFilterList());
		if (cFileName)
		{
			Main_OnCommand(40440,0); // Selected Media Offline
			for (i=0;i<NumActiveTakes;i++)
			{
				CurTake=TheTakes->Get(i);
				ThePCMSource=(PCM_source*)GetSetMediaItemTakeInfo(CurTake,"P_SOURCE",NULL);
			
				if (strcmp(ThePCMSource->GetType(), "SECTION") != 0)
				{
					PCM_source *NewPCMSource = PCM_Source_CreateFromFile(cFileName);
					if (NewPCMSource!=0)
					{
						GetSetMediaItemTakeInfo(CurTake,"P_SOURCE",NewPCMSource);
						delete ThePCMSource;
					}
				}
				else
				{
					PCM_source *TheOtherPCM=ThePCMSource->GetSource();
					if (TheOtherPCM!=0)
					{
						PCM_source *NewPCMSource=PCM_Source_CreateFromFile(cFileName);
						ThePCMSource->SetSource(NewPCMSource);
						delete TheOtherPCM;
					}
				}
			}
			free(cFileName);
			Main_OnCommand(40047,0); // Build any missing peaks
			Main_OnCommand(40439,0); // Selected Media Online
			Undo_OnStateChangeEx("Replace takes source files",4,-1);
			UpdateTimeline();
		}
	}
	delete TheTakes;
}

void DoInvertItemSelection(COMMAND_T*)
{
	for (int i = 1; i <= GetNumTracks(); i++)
	{
		MediaTrack* tr = CSurf_TrackFromID(i, FALSE);
		for (int j = 0; j < GetTrackNumMediaItems(tr); j++)
		{
			MediaItem* CurItem = GetTrackMediaItem(tr, j);
			if (*(bool*)GetSetMediaItemInfo(CurItem, "B_UISEL", NULL))
				GetSetMediaItemInfo(CurItem, "B_UISEL", &g_bFalse);
			else
				GetSetMediaItemInfo(CurItem, "B_UISEL", &g_bTrue);
		}
	}
	UpdateTimeline();
}

bool DoLaunchExternalTool(const char *ExeFilename)
{
	if (!ExeFilename)
		return false;

#ifdef _WIN32
	STARTUPINFO          si = { sizeof(si) };
	PROCESS_INFORMATION  pi;
	char* cFile = _strdup(ExeFilename);
	bool bRet = CreateProcess(0, cFile, 0, 0, FALSE, 0, 0, 0, &si, &pi) ? true : false;
	free(cFile);
	return bRet;
#else
	MessageBox(g_hwndParent, "Not supported on OSX (yet), sorry!", "Unsupported", MB_OK);
	return false;
#endif
}

void DoRepeatPaste(COMMAND_T*)
{
	DialogBox(g_hInst, MAKEINTRESOURCE(IDD_REPEATPASTEDLG), g_hwndParent, RepeatPasteDialogProc);
}

void DoSelectEveryNthItemOnSelectedTracks(int Step,int ItemOffset)
{
	Main_OnCommand(40289,0); // Unselect all items
	
	int flags;
	for (int i = 0; i < GetNumTracks(); i++)
	{
		GetTrackInfo(i,&flags);
		if (flags & 0x02)
		{ 
			MediaTrack* MunRaita = CSurf_TrackFromID(i+1,FALSE);
			int ItemCounter = 0;
			for (int j = 0; j < GetTrackNumMediaItems(MunRaita); j++)
			{
				MediaItem* CurItem = GetTrackMediaItem(MunRaita,j);
				
				if ((ItemCounter % Step) == ItemOffset)
					GetSetMediaItemInfo(CurItem,"B_UISEL",&g_bTrue);
				else
					GetSetMediaItemInfo(CurItem,"B_UISEL",&g_bFalse);	

				ItemCounter++;
			}
		}
	}
	UpdateTimeline();
}

void DoSelectSkipSelectOnSelectedItems(int Step,int ItemOffset)
{
	//
	MediaTrack* MunRaita;
	MediaItem* CurItem;
	
	int numItems;
	
	//Main_OnCommand(40289,0); // Unselect all items
	bool NewSelectedStatus=false;
	int flags; 
	int i;
	int j;
	for (i=0;i<GetNumTracks();i++)
	{
		GetTrackInfo(i,&flags);
		//if (flags & 0x02)
		if (TRUE==TRUE)
		{ 
			MunRaita = CSurf_TrackFromID(i+1,FALSE);
			int ItemCounter=0;
			numItems=GetTrackNumMediaItems(MunRaita);
			for (j=0;j<numItems;j++)
			{
				CurItem = GetTrackMediaItem(MunRaita,j);
				bool ItemSelected=*(bool*)GetSetMediaItemInfo(CurItem,"B_UISEL",NULL);
				if (ItemSelected)
				{
				if ((ItemCounter % Step)==ItemOffset)
				{
					//
					NewSelectedStatus=TRUE;
					GetSetMediaItemInfo(CurItem,"B_UISEL",&NewSelectedStatus);
				} else
				{
					NewSelectedStatus=FALSE;
					GetSetMediaItemInfo(CurItem,"B_UISEL",&NewSelectedStatus);	
				}

				ItemCounter++;
				}


		}
		}
	}
	UpdateTimeline();
}


int NumSteps=2;
int StepOffset=1;
int SkipItemSource=0; // 0 for all in selected tracks, 1 for items selected in selected tracks

WDL_DLGRET SelEveryNthDialogProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	//
	switch(Message)
    {
        case WM_INITDIALOG:
			{
			//SetDlgItemText(hwnd, IDC_EDIT1, "2");
			//SetDlgItemText(hwnd, IDC_EDIT2, "0");
			char TextBuf[32];
			sprintf(TextBuf,"%d",NumSteps);
			
			SetDlgItemText(hwnd, IDC_EDIT1, TextBuf);
			sprintf(TextBuf,"%d",StepOffset);
			SetDlgItemText(hwnd, IDC_EDIT2, TextBuf);
			SetFocus(GetDlgItem(hwnd, IDC_EDIT1));
			SendMessage(GetDlgItem(hwnd, IDC_EDIT1), EM_SETSEL, 0, -1);
			return 0;
			}
        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDOK:
					{
						//MessageBox(g_hwndParent,"LoL","info",MB_OK);
						char TempString[32];
						GetDlgItemText(hwnd,IDC_EDIT1,TempString,31);
						NumSteps=atoi(TempString);
						GetDlgItemText(hwnd,IDC_EDIT2,TempString,31);
						StepOffset=atoi(TempString);
						if (SkipItemSource==0)
							DoSelectEveryNthItemOnSelectedTracks(NumSteps,StepOffset);
						if (SkipItemSource==1)
							DoSelectSkipSelectOnSelectedItems(NumSteps,StepOffset);
						EndDialog(hwnd,0);
						return 0;
					}
				case IDCANCEL:
					{
				
						EndDialog(hwnd,0);
						return 0;
					}
			}
	}
	return 0;
}

void ShowSelectSkipFromSelectedItems(int SelectMode)
{
	SkipItemSource=SelectMode;
	DialogBox(g_hInst, MAKEINTRESOURCE(IDD_SELECT_NTH_ITEMDLG), g_hwndParent, SelEveryNthDialogProc);
}

void DoSkipSelectAllItemsOnTracks(COMMAND_T*)
{
	ShowSelectSkipFromSelectedItems(0);	
}

void DoSkipSelectFromSelectedItems(COMMAND_T*)
{
	ShowSelectSkipFromSelectedItems(1);	
}

int *TakeIndexes;

bool GenerateShuffledTakeRandomTable(int *IntTable,int numItems,int badFirstNumber)
{
	//
	int *CheckTable=new int[1024];
	bool GoodFound=FALSE;
	int IterCount=0;
	int rndInt;
	int i;
	for (i=0;i<1024;i++) 
	{
		CheckTable[i]=0;
		IntTable[i]=0;
	}
	
	for (i=0;i<numItems;i++)
	{
		//
		GoodFound=FALSE;
		while (!GoodFound)
		{
			rndInt=rand() % numItems;
			if ((CheckTable[rndInt]==0) && (rndInt!=badFirstNumber) && (i==0)) GoodFound=TRUE;
			if ((CheckTable[rndInt]==0) && (i>0)) GoodFound=TRUE;
			
			
			IterCount++;
			if (IterCount>1000000) 
			{
				MessageBox(g_hwndParent,"Shuffle Random Table Generator Probably Failed, over 1000000 iterations!","Error",MB_OK);
				break;
			}
		}
		if (GoodFound) 
		{
			IntTable[i]=rndInt;
			CheckTable[rndInt]=1;
		}
		

	}


	delete[] CheckTable;
	return FALSE;

}

void DoShuffleSelectTakesInItems(COMMAND_T*)
{
	//
	MediaTrack* MunRaita;
	MediaItem* CurItem;
	
	int numItems;;
	
	
	bool ItemSelected=false;
	int ValidNumTakesInItems=0;
	int NumSelectedItemsFound=0;
	int TestNumTakes=0;
	bool ValidNumTakes=false;
	int i;
	int j;
	for (i=0;i<GetNumTracks();i++)
	{
		MunRaita = CSurf_TrackFromID(i+1,FALSE);
		numItems=GetTrackNumMediaItems(MunRaita);
		for (j=0;j<numItems;j++)
		{
			CurItem = GetTrackMediaItem(MunRaita,j);
			//propertyName="D_";
			ItemSelected=*(bool*)GetSetMediaItemInfo(CurItem,"B_UISEL",NULL);
			if (ItemSelected==TRUE)
			{
				if (NumSelectedItemsFound==0) 
				{
					ValidNumTakesInItems=GetMediaItemNumTakes(CurItem);
				}
				if (NumSelectedItemsFound>0)
				{
					TestNumTakes = GetMediaItemNumTakes(CurItem);
					if (TestNumTakes != ValidNumTakesInItems)
					{
						ValidNumTakes = false;
						break;
					}
					else
						ValidNumTakes = true;
				}

				NumSelectedItemsFound++;
			}
		}
	}
	int TakeToChoose=0;
	if (!ValidNumTakes)
		MessageBox(g_hwndParent,"Non-valid number of takes in items!","Error",MB_OK);
	
	if (ValidNumTakes)
	{
		TakeIndexes=new int[1024];
		GenerateShuffledTakeRandomTable(TakeIndexes,(ValidNumTakesInItems-0),-1);
		int ShuffledTakesGenerated=0;
		for (i=0;i<GetNumTracks();i++)
		{
			MunRaita = CSurf_TrackFromID(i+1,FALSE);
			numItems=GetTrackNumMediaItems(MunRaita);
			for (j=0;j<numItems;j++)
			{
				CurItem = GetTrackMediaItem(MunRaita,j);
				ItemSelected=*(bool*)GetSetMediaItemInfo(CurItem,"B_UISEL",NULL);
				if (ItemSelected==TRUE)
				{
					// set the take etc
					TakeToChoose=TakeIndexes[ShuffledTakesGenerated];
					if (GetMediaItemNumTakes(CurItem)>0)
					{
						GetSetMediaItemInfo(CurItem,"I_CURTAKE",&TakeToChoose);
						ShuffledTakesGenerated++;
						if (ShuffledTakesGenerated==ValidNumTakesInItems)
						{
							GenerateShuffledTakeRandomTable(TakeIndexes,ValidNumTakesInItems,TakeToChoose);
							ShuffledTakesGenerated=0;
						}
					}
				}
			}
		}
		delete[] TakeIndexes;
		Undo_OnStateChangeEx("Select Takes In Selected Items, Shuffled Random",4,-1);
		UpdateTimeline();
	}
	
}

void DoMoveItemsToEditCursor(COMMAND_T*)
{
	double EditCurPos=GetCursorPosition();
	for (int i = 0; i < CountSelectedMediaItems(0); i++)
	{
		MediaItem* CurItem = GetSelectedMediaItem(0, i);
		double SnapOffset = *(double*)GetSetMediaItemInfo(CurItem, "D_SNAPOFFSET", NULL);
		double NewPos = EditCurPos - SnapOffset;
		GetSetMediaItemInfo(CurItem, "D_POSITION", &NewPos);
	}
	if (CountSelectedMediaItems(0))
	{
		Undo_OnStateChangeEx("Move Selected Items To Edit Cursor", UNDO_STATE_ITEMS, -1);
		UpdateTimeline();
	}
}

void DoRemoveItemFades(COMMAND_T*)
{
	double dFade = 0.0;
	for (int i = 0; i < CountSelectedMediaItems(0); i++)
	{
		MediaItem* item = GetSelectedMediaItem(0, i);
		GetSetMediaItemInfo(item, "D_FADEINLEN",  &dFade);
		GetSetMediaItemInfo(item, "D_FADEOUTLEN", &dFade);
	}
	Undo_OnStateChangeEx("Set Item Fades To 0", UNDO_STATE_ITEMS, -1);
	UpdateTimeline();
}


void DoTrimLeftEdgeToEditCursor(COMMAND_T*)
{
	double NewLeftEdge = GetCursorPosition();
	for (int i = 0; i < CountSelectedMediaItems(0); i++)
	{
		MediaItem* CurItem = GetSelectedMediaItem(0, i);
		double OldLeftEdge = *(double*)GetSetMediaItemInfo(CurItem, "D_POSITION", NULL);
		double OldLength   = *(double*)GetSetMediaItemInfo(CurItem, "D_LENGTH", NULL);
		double NewLength   = OldLength + (OldLeftEdge - NewLeftEdge);
		for (int k = 0; k < GetMediaItemNumTakes(CurItem); k++)
		{
			MediaItem_Take* CurTake = GetMediaItemTake(CurItem, k);
			double OldMediaOffset = *(double*)GetSetMediaItemTakeInfo(CurTake, "D_STARTOFFS", NULL);
			double NewMediaOffset = OldMediaOffset - (OldLeftEdge - NewLeftEdge);
			GetSetMediaItemTakeInfo(CurTake,"D_STARTOFFS",&NewMediaOffset);
		}

		GetSetMediaItemInfo(CurItem, "D_POSITION", &NewLeftEdge);
		GetSetMediaItemInfo(CurItem, "D_LENGTH", &NewLength);
	}
	if (CountSelectedMediaItems(0))
	{
		Undo_OnStateChangeEx("Trim/Untrim Item Left Edge", UNDO_STATE_ITEMS, -1);
		UpdateTimeline();
	}
}

void DoTrimRightEdgeToEditCursor(COMMAND_T*)
{
	double dRightEdge = GetCursorPosition();
	for (int i = 0; i < CountSelectedMediaItems(0); i++)
	{
		MediaItem* CurItem = GetSelectedMediaItem(0, i);
		double LeftEdge  = *(double*)GetSetMediaItemInfo(CurItem, "D_POSITION", NULL);
		double NewLength = dRightEdge - LeftEdge;
		GetSetMediaItemInfo(CurItem, "D_LENGTH", &NewLength);
	}
	//if (CountSelectedMediaItems(0))
	//{
	//	Undo_OnStateChangeEx("Trim/Untrim Item Right Edge", UNDO_STATE_ITEMS, -1);
	//	UpdateTimeline();
	//}
}

void DoResetItemRateAndPitch(COMMAND_T*)
{
	Undo_BeginBlock();
	Main_OnCommand(40652, 0); // set item rate to 1.0
	Main_OnCommand(40653, 0); // reset item pitch to 0.0
	Undo_EndBlock("Reset Item Pitch And Rate",0);
}

void DoApplyTrackFXStereoAndResetVol(COMMAND_T*)
{
	Undo_BeginBlock();
	Main_OnCommand(40209,0); // apply track fx in stereo to items
	double dVol = 1.0;
	for (int i = 0; i < CountSelectedMediaItems(0); i++)
		GetSetMediaItemInfo(GetSelectedMediaItem(0, i), "D_VOL", &dVol);

	Undo_EndBlock("Apply Track FX To Items And Reset Volume", UNDO_STATE_ITEMS);
}

void DoApplyTrackFXMonoAndResetVol(COMMAND_T*)
{
	Undo_BeginBlock();
	Main_OnCommand(40361,0); // apply track fx in mono to items
	double dVol = 1.0;
	for (int i = 0; i < CountSelectedMediaItems(0); i++)
		GetSetMediaItemInfo(GetSelectedMediaItem(0, i), "D_VOL", &dVol);
	Undo_EndBlock("Apply Track FX To Items (Mono) And Reset Volume", UNDO_STATE_ITEMS);
}

void AnalyzePCMSourceForPeaks(PCM_source *pSrc, double *dPeakL, double *dPeakR, double *dRMS_L, double *dRMS_R, INT64* peakSample)
{
	const int iFrameLen = 1024;
	double* buf = new double[iFrameLen * pSrc->GetNumChannels()];
	double sumSquaresL = 0.0;
	double sumSquaresR = 0.0;
	*dPeakL = 0.0;
	*dPeakR = 0.0;
	double dPeak = 0.0;
	
	PCM_source_transfer_t transferBlock={0,};
	transferBlock.length = iFrameLen;
	transferBlock.samplerate = pSrc->GetSampleRate();
	transferBlock.samples = buf;
	transferBlock.nch = pSrc->GetNumChannels();

	INT64 sampleCounter = 0;
	int iFrame = 0;

	do
	{
		transferBlock.time_s = (double)iFrameLen * iFrame++ / pSrc->GetSampleRate();
		pSrc->GetSamples(&transferBlock);

		if (pSrc->GetNumChannels() == 1)
		{
			for (int j = 0; j < transferBlock.samples_out; j++)
			{
				sumSquaresL += buf[j] * buf[j];
				if (fabs(buf[j]) > *dPeakL)
				{
					*dPeakL = fabs(buf[j]);
					if (peakSample)
						*peakSample = sampleCounter;
				}
				sampleCounter++;
			}
		}		
		else if (pSrc->GetNumChannels() == 2)
		{
			for (int j = 0; j < transferBlock.samples_out; j++)
			{
				sumSquaresL += buf[j*2]   * buf[j*2];
				sumSquaresR += buf[j*2+1] * buf[j*2+1];
				double dAbsL = fabs(buf[j*2]);
				double dAbsR = fabs(buf[j*2+1]);

				if (dAbsL > *dPeakL)
					*dPeakL = dAbsL;
				if (dAbsR > *dPeakR)
					*dPeakR = dAbsR;
				if (peakSample)
				{
					if (dAbsL > dPeak)
					{
						dPeak = dAbsL;
						*peakSample = sampleCounter;
					}
					if (dAbsR > dPeak)
					{
						dPeak = dAbsR;
						*peakSample = sampleCounter;
					}
				}
				sampleCounter++;
			}
		}
	}
	while (transferBlock.samples_out);

	if (dRMS_L)
		*dRMS_L = sqrt(sumSquaresL / sampleCounter);
	if (dRMS_R)
		*dRMS_R = sqrt(sumSquaresR / sampleCounter);
	delete[] buf;
}


void DoAnalyzeAndShowPeakInItemMedia(COMMAND_T*)
{
	for (int i = 0; i < CountSelectedMediaItems(NULL); i++)
	{
		MediaItem* mi = GetSelectedMediaItem(NULL, i);
		PCM_source* pSrc = (PCM_source*)mi;
		if (strcmp(pSrc->GetType(), "MIDI") != 0)
		{
			pSrc = pSrc->Duplicate();
			if (pSrc != NULL)
			{
				double dPeakL;
				double dPeakR;
				double rmsL;
				double rmsR;

				double dZero = 0.0;
				GetSetMediaItemInfo((MediaItem*)pSrc, "D_POSITION", &dZero);
				
				// Do the work!
				AnalyzePCMSourceForPeaks(pSrc, &dPeakL, &dPeakR, &rmsL, &rmsR, NULL);

				char MesBuf[256];
				if (pSrc->GetNumChannels() == 1)
					sprintf(MesBuf,"Peak level of mono item = %f dB\nRMS level of mono item= %.2f dB",
					  VAL2DB(dPeakL), VAL2DB(rmsL));
				else if (pSrc->GetNumChannels() == 2)
					sprintf(MesBuf,"Peak levels for stereo item, Left = %.2f dB , Right = %.2f dB\nRMS levels of stereo item, Left=%.2f dB, Right =%.2f dB",
					  VAL2DB(dPeakL), VAL2DB(dPeakR), VAL2DB(rmsL), VAL2DB(rmsR));
				else
					sprintf(MesBuf, "Non-supported channel count!");
				MessageBox(g_hwndParent, MesBuf, "Item peak gain", MB_OK);
				delete pSrc;
			}
		}
	}
}

void DoFindItemPeak(COMMAND_T*)
{
	// Just use the first item
	if (CountSelectedMediaItems(NULL))
	{
		MediaItem* mi = GetSelectedMediaItem(NULL, 0);
		PCM_source* pSrc = (PCM_source*)mi;
		if (strcmp(pSrc->GetType(), "MIDI") != 0)
		{
			pSrc = pSrc->Duplicate();
			if (pSrc)
			{
				double dPeakL;
				double dPeakR;
				INT64 iPeakSample = 0;

				double dZero = 0.0;
				GetSetMediaItemInfo((MediaItem*)pSrc, "D_POSITION", &dZero);
				// Do the work!
				AnalyzePCMSourceForPeaks(pSrc, &dPeakL, &dPeakR, NULL, NULL, &iPeakSample);

				if (iPeakSample)
				{
					double dSrate = pSrc->GetSampleRate();
					double dPos = *(double*)GetSetMediaItemInfo(mi, "D_POSITION", NULL);
					dPos += iPeakSample / dSrate;
					SetEditCurPos(dPos, true, false);
				}
			}
		}
	}
}

void DoSelItemsToEndOfTrack(COMMAND_T*)
{
	MediaTrack* MunRaita;
	MediaItem* CurItem;
	int numItems;
	bool ItemSelected=false;
	int flags;
	int i;
	int j;
	for (i=0;i<GetNumTracks();i++)
	{
		MunRaita = CSurf_TrackFromID(i+1,FALSE);
		GetTrackInfo(i,&flags);
		//if (flags & 0x02)
		//{ 
		numItems=GetTrackNumMediaItems(MunRaita);
		int LastSelItemIndex=-1;
		for (j=0;j<numItems;j++)
		{
			CurItem = GetTrackMediaItem(MunRaita,j);
			//propertyName="D_";
			ItemSelected=*(bool*)GetSetMediaItemInfo(CurItem,"B_UISEL",NULL);
			if (ItemSelected==TRUE)
			{
				LastSelItemIndex=j;
				break;
			}


		}
		if (LastSelItemIndex>=0)
		{
			//
			for (j=0;j<GetTrackNumMediaItems(MunRaita);j++)
			{
				CurItem = GetTrackMediaItem(MunRaita,j);
				if (j>=LastSelItemIndex && j<GetTrackNumMediaItems(MunRaita))
					ItemSelected=true; else ItemSelected=false;
				
				GetSetMediaItemInfo(CurItem,"B_UISEL",&ItemSelected);
			}

		}
		//}


	}
	UpdateTimeline();
}

void DoSelItemsToStartOfTrack(COMMAND_T*)
{
	MediaTrack* MunRaita;
	MediaItem* CurItem;
	int numItems;
	bool ItemSelected=false;
	int flags;
	int j;
	int i;
	for (i=0;i<GetNumTracks();i++)
	{
		MunRaita = CSurf_TrackFromID(i+1,FALSE);
		GetTrackInfo(i,&flags);
		//if (flags & 0x02)
		//{ 
		numItems=GetTrackNumMediaItems(MunRaita);
		int LastSelItemIndex=-1;
		for (j=0;j<numItems;j++)
		{
			CurItem = GetTrackMediaItem(MunRaita,j);
			//propertyName="D_";
			ItemSelected=*(bool*)GetSetMediaItemInfo(CurItem,"B_UISEL",NULL);
			if (ItemSelected==TRUE)
			{
				LastSelItemIndex=j;
				//break;
			}


		}
		if (LastSelItemIndex>=0)
		{
			//
			for (j=0;j<GetTrackNumMediaItems(MunRaita);j++)
			{
				CurItem = GetTrackMediaItem(MunRaita,j);
				if (j>=0 && j<=LastSelItemIndex)
					ItemSelected=true; else ItemSelected=false;
				
				GetSetMediaItemInfo(CurItem,"B_UISEL",&ItemSelected);
			}

		}
		//}


	}
	UpdateTimeline();
}

void DoSetAllTakesPlay()
{
	//
	MediaTrack* MunRaita;
	MediaItem* CurItem;
	//MediaItem_Take* CurTake;
	int numItems;;
	
	
	bool ItemSelected=false;
	
	int i;
	int j;
	
	for (i=0;i<GetNumTracks();i++)
	{
		MunRaita = CSurf_TrackFromID(i+1,FALSE);
		numItems=GetTrackNumMediaItems(MunRaita);
		for (j=0;j<numItems;j++)
		{
			CurItem = GetTrackMediaItem(MunRaita,j);
			//propertyName="D_";
			ItemSelected=*(bool*)GetSetMediaItemInfo(CurItem,"B_UISEL",NULL);
			if (ItemSelected==TRUE)
			{
				bool PlayAllTakes=true;
				GetSetMediaItemInfo(CurItem,"B_ALLTAKESPLAY",&PlayAllTakes);
				
			} 

		}
	}
	//Undo_OnStateChangeEx("Trim/Untrim Item Right Edge",4,-1);
	//UpdateTimeline();
}


void DoPanTakesOfItemSymmetrically()
{
	//
	MediaTrack* MunRaita;
	MediaItem* CurItem;
	MediaItem_Take* CurTake;
	int numItems;
	bool ItemSelected=false;
	int i;
	int j;
	int k;
	for (i=0;i<GetNumTracks();i++)
	{
		MunRaita = CSurf_TrackFromID(i+1,FALSE);
		numItems=GetTrackNumMediaItems(MunRaita);
		for (j=0;j<numItems;j++)
		{
			CurItem = GetTrackMediaItem(MunRaita,j);
			ItemSelected=*(bool*)GetSetMediaItemInfo(CurItem,"B_UISEL",NULL);
			if (ItemSelected==TRUE)
			{
				if (GetMediaItemNumTakes(CurItem)>0)
				{
					int NumTakes=GetMediaItemNumTakes(CurItem);
					for (k=0;k<NumTakes;k++)
					{
						CurTake=GetMediaItemTake(CurItem,k);
					
						//double SnapOffset=*(double*)GetSetMediaItemInfo(CurItem,"D_SNAPOFFSET",NULL);
						double NewPan=-1.0+((2.0/(NumTakes-1))*k);
											
						GetSetMediaItemTakeInfo(CurTake,"D_PAN",&NewPan);
					}
				}
			} 
		}
	}
}

void DoPanTakesSymmetricallyWithUndo(COMMAND_T*)
{
	DoPanTakesOfItemSymmetrically();
	Undo_OnStateChangeEx("Set Pan Of Takes In Item",4,-1);
	UpdateTimeline();
}


void DoImplodeTakesSetPlaySetSymPans(COMMAND_T*)
{
	Undo_BeginBlock();
	Main_OnCommand(40438,0); // implode items across tracks into takes
	DoSetAllTakesPlay();
	DoPanTakesOfItemSymmetrically();
	Undo_EndBlock("Implode Items And Pan Symmetrically",0);
	//UpdateTimeline();
}

double g_lastTailLen;

void DoWorkForRenderItemsWithTail(double TailLen)
{
	// Unselect all tracks
	
	MediaTrack* MunRaita;
	MediaItem* CurItem;
	
	int numItems;;
	
	char textbuf[100];
	int sz=0; int *fxtail = (int *)get_config_var("itemfxtail",&sz);
    int OldTail=*fxtail;
	if (sz==sizeof(int) && fxtail) 
	{ 
			
		int msTail=int(1000*TailLen);
		*fxtail=msTail;
		sprintf(textbuf,"%d",msTail);
	}

	bool ItemSelected=false;
	int i;
	int j;
	for (i=0;i<GetNumTracks();i++)
	{
		MunRaita = CSurf_TrackFromID(i+1,FALSE);
		numItems=GetTrackNumMediaItems(MunRaita);
		Main_OnCommand(40635,0);
		for (j=0;j<numItems;j++)
		{
			CurItem = GetTrackMediaItem(MunRaita,j);
			//propertyName="D_";
			ItemSelected=*(bool*)GetSetMediaItemInfo(CurItem,"B_UISEL",NULL);
			if (ItemSelected==TRUE)
			{
				Main_OnCommand(40601,0); // render items as new take
			} 
		}
	}
	*fxtail=OldTail;
	UpdateTimeline();
}

WDL_DLGRET RenderItemsWithTailDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	//
	switch(Message)
    {
        case WM_INITDIALOG:
		{
			char TextBuf[32];
			
			sprintf(TextBuf,"%.2f",g_lastTailLen);
			SetDlgItemText(hwnd, IDC_EDIT1, TextBuf);
			SetFocus(GetDlgItem(hwnd, IDC_EDIT1));
			SendMessage(GetDlgItem(hwnd, IDC_EDIT1), EM_SETSEL, 0, -1);
			break;
		}
        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDOK:
				{
					char textbuf[100];
					GetDlgItemText(hwnd,IDC_EDIT1,textbuf,100);
					g_lastTailLen=atof(textbuf);
					DoWorkForRenderItemsWithTail(g_lastTailLen);
					EndDialog(hwnd,0);
					break;
				}
				case IDCANCEL:
					EndDialog(hwnd,0);
					break;
			}
	}
	return 0;
}

void DoRenderItemsWithTail(COMMAND_T*)
{
	DialogBox(g_hInst, MAKEINTRESOURCE(IDD_RENDITEMS), g_hwndParent, RenderItemsWithTailDlgProc);	
}

void DoOpenAssociatedRPP(COMMAND_T*)
{
	WDL_PtrList<MediaItem_Take> *TheTakes=new (WDL_PtrList<MediaItem_Take>);
	PCM_source *ThePCMSource;
	MediaItem_Take* CurTake;
	int NumActiveTakes=GetActiveTakes(TheTakes);
	if (NumActiveTakes==1)
	{
		CurTake=TheTakes->Get(0); // we will only support first selected item for now
		ThePCMSource=(PCM_source*)GetSetMediaItemTakeInfo(CurTake,"P_SOURCE",NULL);
		char RPPFileNameBuf[1024];

		sprintf(RPPFileNameBuf,"%s\\reaper.exe \"%s.RPP\"",GetExePath(), ThePCMSource->GetFileName());
		if (!DoLaunchExternalTool(RPPFileNameBuf))
			MessageBox(g_hwndParent,"Could not launch REAPER.","Error",MB_OK);
	}
	else
		MessageBox(g_hwndParent,"None or more than 1 item selected","Error",MB_OK);
	delete TheTakes;
}

typedef struct 
{
	double Gap;
	int ModeA;
	int ModeB;
	int ModeC;
} t_ReposItemsParams;

t_ReposItemsParams g_ReposItemsParams;

bool g_FirstReposItemsRun = true;

void RepositionItems(double theGap,int ModeA,int ModeB,int ModeC) // ModeA : gap from item starts/end... ModeB=per track/all items...ModeC=seconds/beats...
{
	//
	MediaTrack* MunRaita;
	MediaItem* CurItem;
	
	int numItems;
	bool ItemSelected=false;
	int PrevSelItemInd=-1;
	int FirstSelItemInd=-1;
	int i;
	int j;
	for (i=0;i<GetNumTracks();i++)
	{
		MunRaita = CSurf_TrackFromID(i+1,FALSE);
		numItems=GetTrackNumMediaItems(MunRaita);
		//MediaItem* **MediaItemsOnTrack = new (MediaItem*)[numItems];
		MediaItem** MediaItemsOnTrack = new MediaItem*[numItems];
		
		for (j=0;j<numItems;j++)
		{
			CurItem = GetTrackMediaItem(MunRaita,j);
			MediaItemsOnTrack[j]=CurItem;
			bool X;
			X=*(bool*)GetSetMediaItemInfo(CurItem,"B_UISEL",NULL);
			if ((FirstSelItemInd==-1) && (X==true)) FirstSelItemInd=j;
		}
		PrevSelItemInd=FirstSelItemInd;
		for (j=0;j<numItems;j++)
		{
			ItemSelected=*(bool*)GetSetMediaItemInfo(MediaItemsOnTrack[j],"B_UISEL",NULL);
			if (ItemSelected==TRUE)
			{
				//double SnapOffset=*(double*)GetSetMediaItemInfo(CurItem,"D_SNAPOFFSET",NULL);
				double NewPos;
				double OldPos=*(double*)GetSetMediaItemInfo(MediaItemsOnTrack[j],"D_POSITION",NULL);
				//double OldPos=g_StoredPositions[ItemCounter];
				if (ModeA==0)
				{
					if (j==FirstSelItemInd) NewPos=OldPos;
					if (j>FirstSelItemInd) 
					{
						OldPos=*(double*)GetSetMediaItemInfo(MediaItemsOnTrack[PrevSelItemInd],"D_POSITION",NULL);
						NewPos=OldPos+theGap;
					}
				
				}
				if (ModeA==1)
				{
					if (j==FirstSelItemInd) NewPos=OldPos;
					if (j>FirstSelItemInd) 
					{
						OldPos=*(double*)GetSetMediaItemInfo(MediaItemsOnTrack[PrevSelItemInd],"D_POSITION",NULL);
						double PrevLen=*(double*)GetSetMediaItemInfo(MediaItemsOnTrack[PrevSelItemInd],"D_LENGTH",NULL);
						double PrevEnd=OldPos+PrevLen;
						NewPos=PrevEnd+theGap;
					}
				}
								
				GetSetMediaItemInfo(MediaItemsOnTrack[j],"D_POSITION",&NewPos);
				PrevSelItemInd=j;
			} 
		}
		delete[] MediaItemsOnTrack;
	}
}


WDL_DLGRET ReposItemsDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch(Message)
    {
		case WM_INITDIALOG:
		{
			if (g_ReposItemsParams.ModeA == 0)
				CheckDlgButton(hwnd, IDC_RADIO1, BST_CHECKED);
			else if (g_ReposItemsParams.ModeA == 1)
				CheckDlgButton(hwnd, IDC_RADIO2, BST_CHECKED);
			char TextBuf[32];
			
			sprintf(TextBuf,"%.2f",g_ReposItemsParams.Gap);
			SetDlgItemText(hwnd, IDC_EDIT1, TextBuf);
			SetFocus(GetDlgItem(hwnd, IDC_EDIT1));
			SendMessage(GetDlgItem(hwnd, IDC_EDIT1), EM_SETSEL, 0, -1);
			break;
		}
        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDOK:
				{
					char textbuf[30];
					GetDlgItemText(hwnd,IDC_EDIT1,textbuf,30);
					double theGap=atof(textbuf);
					int modeA=0;
					if (IsDlgButtonChecked(hwnd,IDC_RADIO1) == BST_CHECKED) modeA = 0;
					if (IsDlgButtonChecked(hwnd,IDC_RADIO2) == BST_CHECKED) modeA = 1;
					RepositionItems(theGap,modeA,0,0);
					UpdateTimeline();
					Undo_OnStateChangeEx("Reposition Items",4,-1);
					g_ReposItemsParams.Gap=theGap;
					g_ReposItemsParams.ModeA=modeA;
					g_ReposItemsParams.ModeB=0;
					g_ReposItemsParams.ModeC=0;
					
					EndDialog(hwnd,0);
					break;
				}
				case IDCANCEL:
					EndDialog(hwnd,0);
					break;
			}
	}
	return 0;
}

void DoReposItemsDlg(COMMAND_T*)
{
	//
	if (g_FirstReposItemsRun==true)
	{
		g_ReposItemsParams.Gap=1.0;
		g_ReposItemsParams.ModeA=1; // item starts based
		g_ReposItemsParams.ModeB=0; // per track based
		g_ReposItemsParams.ModeC=0; // seconds
		g_FirstReposItemsRun=false;
	}

	DialogBox(g_hInst, MAKEINTRESOURCE(IDD_REPOSITEMS), g_hwndParent, ReposItemsDlgProc);
}

void DoSpeadSelItemsOverNewTx(COMMAND_T*)
{
	vector<MediaItem*> TheItems;
	XenGetProjectItems(TheItems,true,false);
	if (TheItems.size()>1)
	{
		MediaTrack* OriginTrack=(MediaTrack*)GetSetMediaItemInfo(TheItems[0],"P_TRACK",NULL);
		int OriginTrackID=CSurf_TrackToID(OriginTrack,false);
		int newDep=1;
		GetSetMediaTrackInfo(OriginTrack,"I_FOLDERDEPTH",&newDep);
		int i;
		// we make new tracks first to not make reaper or ourselves confused
		for (i=0;i<(int)TheItems.size();i++)
			InsertTrackAtIndex(OriginTrackID+i+1,true);

		// then we "drop" the items
		OriginTrack=(MediaTrack*)GetSetMediaItemInfo(TheItems[0],"P_TRACK",NULL);
		OriginTrackID=CSurf_TrackToID(OriginTrack,false);
		
		MediaTrack* DestTrack=0;
		for (i=0;i<(int)TheItems.size();i++)
		{
			int NewTrackIndex=OriginTrackID+i+1;
			DestTrack=CSurf_TrackFromID(NewTrackIndex,false);
			if (DestTrack)
				MoveMediaItemToTrack(TheItems[i],DestTrack);
		}
		newDep=-1;
		GetSetMediaTrackInfo(DestTrack,"I_FOLDERDEPTH",&newDep);
		TrackList_AdjustWindows(false);
		Undo_OnStateChangeEx("Spread selected items on new tracks",UNDO_STATE_ALL,-1);

	} else MessageBox(g_hwndParent,"No or only one item selected!","Error",MB_OK);
		
}

int OpenInExtEditor(int editorIdx)
{
	vector<MediaItem_Take*> TheTakes;
	XenGetProjectTakes(TheTakes,true,true);
	if (TheTakes.size()==1)
	{
		MediaItem* CurItem=(MediaItem*)GetSetMediaItemTakeInfo(TheTakes[0],"P_ITEM",NULL);
		//
		
		//Main_OnCommand(40639,0); // duplicate active take
		Main_OnCommand(40601,0); // render items and add as new take
		//int curTakeIndex=*(int*)GetSetMediaItemInfo(CurItem,"I_CURTAKE",NULL);
		MediaItem_Take *CopyTake=GetMediaItemTake(CurItem,-1);
		PCM_source *ThePCM=(PCM_source*)GetSetMediaItemTakeInfo(CopyTake,"P_SOURCE",NULL);
		char ExeString[2048];
		if (editorIdx==0 && g_external_app_paths.PathToAudioEditor1)
			sprintf(ExeString,"\"%s\" \"%s\"",g_external_app_paths.PathToAudioEditor1,ThePCM->GetFileName());
		else if (editorIdx==1 && g_external_app_paths.PathToAudioEditor2)
			sprintf(ExeString,"\"%s\" \"%s\"",g_external_app_paths.PathToAudioEditor2,ThePCM->GetFileName());
		
		DoLaunchExternalTool(ExeString);
	}

	return -666;	
}

void DoOpenInExtEditor1(COMMAND_T*) { OpenInExtEditor(0); }
void DoOpenInExtEditor2(COMMAND_T*) { OpenInExtEditor(1); }

void DoMatrixItemImplode(COMMAND_T*)
{
	vector<MediaItem*> TheItems;
	XenGetProjectItems(TheItems,true,false);
	vector<double> OldItemPositions;
	vector<double> OlddItemLens;
	int i;
	for (i=0;i<(int)TheItems.size();i++)
	{
		double itemPos=*(double*)GetSetMediaItemInfo(TheItems[i],"D_POSITION",NULL);
		OldItemPositions.push_back(itemPos);
		double dItemLen=*(double*)GetSetMediaItemInfo(TheItems[i],"D_LENGTH",NULL);
		OlddItemLens.push_back(dItemLen);
	}
	//Main_OnCommand(40289,0); // unselect all
	Undo_BeginBlock();
	Main_OnCommand(40543,0); // implode selected items to item
	Main_OnCommand(40698,0); // copy item
	vector<MediaItem*> PastedItems;
	vector<MediaItem*> TempItem; // oh fuck what shit this is
	for (i=1;i<(int)OldItemPositions.size();i++)
	{
		SetEditCurPos(OldItemPositions[i],false,false);
		Main_OnCommand(40058,0);
		XenGetProjectItems(TempItem,true,false); // basically a way to get the first selected item, messsyyyyy
		double newdItemLen=OlddItemLens[i];
		GetSetMediaItemInfo(TempItem[0],"D_LENGTH",&newdItemLen);
		//PastedItems.push_back(TempItem[0]);
	}
	/*
	for (i=0;i<PastedItems.size();i++)
	{
		double newdItemLen=OlddItemLens[i+1];
		GetSetMediaItemInfo(PastedItems[i],"D_LENGTH",&newdItemLen);
	}
	*/
	//40058 // paste
	Undo_EndBlock("Matrix implode items",0);
	//Undo_OnStateChangeEx("Matrix implode items",4,-1);
}

double g_swingBase=1.0/4.0;
double g_swingAmt=0.2;

void PerformSwingItemPositions(double swingBase,double swingAmt)
{
	// 14th October 2009 (X)
	// too bad this doesn't work so well
	// maybe someone could make it work :)
	vector<MediaItem*> TheItems;
	XenGetProjectItems(TheItems,true,false);
	int i;
	//double temvo=TimeMap_GetDividedBpmAtTime(0.0);
	for (i=0;i<(int)TheItems.size();i++)
	{
		double itempos=*(double*)GetSetMediaItemInfo(TheItems[i],"D_POSITION",0);
		double itemposQN=TimeMap_timeToQN(itempos);
		int itemposSXTHN=(int)((itemposQN*4.0)+0.5);
		//double itemposinsixteenths=itemposQN*4.0;
		int oddOreven=itemposSXTHN % 2;
		double yay=fabs(swingAmt);
		double newitemposQN=itemposQN;
		if (swingAmt>0) 
			newitemposQN=itemposQN+(1.0/4.0*yay); // to right
		if (swingAmt<0)
			newitemposQN=itemposQN-(1.0/4.0*yay); // to left
		itempos=TimeMap_QNToTime(newitemposQN);
		if (oddOreven==1)
			GetSetMediaItemInfo(TheItems[i],"D_POSITION",&itempos);
	}
	UpdateTimeline();
	Undo_OnStateChangeEx("Swing item positions",4,-1);
}

WDL_DLGRET SwingItemsDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	char tbuf[200];
	if (Message==WM_INITDIALOG)
	{
		sprintf(tbuf,"%.2f",g_swingAmt*100.0);
		SetDlgItemText(hwnd,IDC_EDIT1,tbuf);
		return 0;
	}
	if (Message==WM_COMMAND && LOWORD(wParam)==IDOK)
	{
		GetDlgItemText(hwnd,IDC_EDIT1,tbuf,199);
		g_swingAmt=atof(tbuf)/100.0;
		if (g_swingAmt<-0.95) g_swingAmt=-0.95; // come on let's be sensible, why anyone would swing more, or even close to this!
		if (g_swingAmt>0.95) g_swingAmt=0.95;
		PerformSwingItemPositions(g_swingBase,g_swingAmt);
		EndDialog(hwnd,0);
		return 0;
	}
	if (Message==WM_COMMAND && LOWORD(wParam)==IDCANCEL)
	{
		EndDialog(hwnd,0);
		return 0;
	}
	return 0;
}

void DoSwingItemPositions(COMMAND_T*)
{
	DialogBox(g_hInst, MAKEINTRESOURCE(IDD_SWINGITEMPOS), g_hwndParent, SwingItemsDlgProc);
}

void DoTimeSelAdaptDelete(COMMAND_T*)
{
	vector<MediaItem*> SelItems;
	vector<MediaItem*> ItemsInTimeSel;
	XenGetProjectItems(SelItems,true,false);
	double OldTimeSelLeft=0.0;
	double OldTimeSelRight=0.0;
	GetSet_LoopTimeRange(false,false,&OldTimeSelLeft,&OldTimeSelRight,false);
	int i;
	if (OldTimeSelRight-OldTimeSelLeft>0.0)
	{
		for (i=0;i<(int)SelItems.size();i++)
		{
			double itempos=*(double*)GetSetMediaItemInfo(SelItems[i],"D_POSITION",0);
			double dItemLen=*(double*)GetSetMediaItemInfo(SelItems[i],"D_LENGTH",0);
			bool itemsel=*(bool*)GetSetMediaItemInfo(SelItems[i],"B_UISEL",0);
			if (itemsel)
			{
				int intersectmatches=0;
				if (OldTimeSelLeft>=itempos && OldTimeSelRight<=itempos+dItemLen)
					intersectmatches++;
				if (itempos>=OldTimeSelLeft && itempos+dItemLen<=OldTimeSelRight)
					intersectmatches++;
				if (OldTimeSelLeft<=itempos+dItemLen && OldTimeSelRight>=itempos+dItemLen)
					intersectmatches++;
				if (OldTimeSelRight>=itempos && OldTimeSelLeft<itempos)
					intersectmatches++;
				if (intersectmatches>0)
					ItemsInTimeSel.push_back(SelItems[i]);
			}
		}
	}
	// 40312
	if (ItemsInTimeSel.size()>0)
	{
		Main_OnCommand(40312,0);
	}
	if (ItemsInTimeSel.size()==0)
	{
		Main_OnCommand(40006,0);
	}
}

void DoDeleteMutedItems(COMMAND_T*)
{
	
	Undo_BeginBlock();
	Main_OnCommand(40289,0); // unselect all items
	vector<MediaItem*> pitems;
	XenGetProjectItems(pitems,false,false);
	int i;
	for (i=0;i<(int)pitems.size();i++)
	{
		bool muted=*(bool*)GetSetMediaItemInfo(pitems[i],"B_MUTE",0);
		if (muted)
		{
			bool uisel=true;
			GetSetMediaItemInfo(pitems[i],"B_UISEL",&uisel);
		}
	}
	Main_OnCommand(40006,0);
	Undo_EndBlock("Remove muted items",0);
	UpdateTimeline();
}