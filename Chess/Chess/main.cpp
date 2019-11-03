#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <TCHAR.H>
#include <Commctrl.h>
#include <stdio.h>
#include "resource.h"
#include "macro_table.h"
#include "pieces.h"
#include "player.h"

#define WM_ASYNC WM_USER+2

class IPv4
{
public:unsigned char b1, b2, b3, b4;
};

bool getMyIP(IPv4 & myIP);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK DlgProc1(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK DlgProc2(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK DlgProc3(HWND, UINT, WPARAM, LPARAM);
void init();
void Restart();

HINSTANCE g_hInst;
HWND hWndMain;
LPCTSTR lpszClass = _T("Chess V1.0");

int **board,**moveP;
bool isServer = false, isTurn;
POINT poin;
TCHAR ServerIP[20];
static SOCKET s, cs;
POINT bpoint;

PLAYER Players[2];
HBITMAP AllPieces[2][6];
//0:나 1:상대편
//0:폰 1:나이트 2:비숍 3:룩 4:퀸 5:킹

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance
	, LPSTR lpszCmdParam, int nCmdShow)
{
	HWND hWnd;
	MSG Message;
	WNDCLASS WndClass;
	g_hInst = hInstance;
	HACCEL hAccel = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ACCELERATOR1));

	WndClass.cbClsExtra = 0;
	WndClass.cbWndExtra = 0;
	WndClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	WndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	WndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	WndClass.hInstance = hInstance;
	WndClass.lpfnWndProc = WndProc;
	WndClass.lpszClassName = lpszClass;
	WndClass.lpszMenuName = NULL;
	WndClass.style = CS_HREDRAW | CS_VREDRAW;
	RegisterClass(&WndClass);

	hWnd = CreateWindow(lpszClass, lpszClass, WS_OVERLAPPEDWINDOW,
		0, 0, 640, 640,
		NULL, (HMENU)NULL, hInstance, NULL);
	ShowWindow(hWnd, nCmdShow);
	hWndMain = hWnd; // hWnd 정보도 전역변수에 저장!

	while (GetMessage(&Message, NULL, 0, 0)) {
		if (!TranslateAccelerator(hWnd, hAccel, &Message)) {
			TranslateMessage(&Message);
			DispatchMessage(&Message);
		}
	}
	return (int)Message.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage,
	WPARAM wParam, LPARAM lParam)
{
	HDC hdc;
	PAINTSTRUCT ps;
	int i, j;
	HBRUSH hBrush, hOldBrush;
	COLORREF color;
	HDC MemDC;
	HBITMAP hBit,OldBitmap;
	BITMAP bit;
	POINT tmp;
	bool  isCheck = false;

	static HWND hDlg=NULL;
	static RECT board_R;
	static bool isCanMove, isLose = false, isWin = false, isInit = false,isWarn=true;
	static WSADATA wsadata;
	static SOCKADDR_IN addr = {0},c_addr;

	static TCHAR msg[200];
	static char buffer[100];
	static TCHAR str[100];
	static int count;
	static int size, msgLen;

	switch (iMessage) {
	case WM_CREATE:
		isCanMove = false;
		board = new int*[8];
		moveP = new int*[8];
		for (i = 0;i < 8;i++) {
			board[i] = new int[8];
			moveP[i] = new int[8];
		}
		board_R.top = board_R.left = 20;
		board_R.bottom = board_R.right = 580;
		DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DIALOG2), hWnd, DlgProc2);
		WSAStartup(MAKEWORD(2, 2), &wsadata);
		s = socket(AF_INET, SOCK_STREAM, 0);
		addr.sin_family = AF_INET;
		addr.sin_port = 1217;

		WideCharToMultiByte(CP_ACP, 0, ServerIP, 20, buffer, 20, NULL, NULL);
		addr.sin_addr.s_addr = inet_addr(buffer);
		if (isServer) {
			bind(s, (LPSOCKADDR)&addr, sizeof(addr));
			WSAAsyncSelect(s, hWnd, WM_ASYNC, FD_ACCEPT);

			if (listen(s, 5) == -1)
				return 0;
		}
		else {
			WSAAsyncSelect(s, hWnd, WM_ASYNC, FD_READ);
			if (connect(s, (LPSOCKADDR)&addr, sizeof(addr)) == -1)
				return 0;
		}
		
		hWndMain = hWnd;
		return 0;
	case WM_COMMAND:
		if (MessageBox(hWnd, _T("게임을 그만두고 항복하시겠습니까?"), _T("항복하시겠습니까?"), MB_YESNO) == IDYES) {
			sprintf(buffer, "Q");
			if (isServer) {
				send(cs, (LPSTR)buffer, strlen(buffer) + 1, 0);
			}
			else {
				send(s, (LPSTR)buffer, strlen(buffer) + 1, 0);
			}
			if (MessageBox(hWnd, _T("아쉽습니다... 게임에서 패배하였습니다. 한판더 하시겠습니까?"), _T("Game Over"), MB_YESNO) == IDYES) {
				Restart();
			}
		}
		return 0;
	case WM_ASYNC:
		switch (lParam)
		{
		case FD_ACCEPT:
			isInit = true;
			if (hDlg != NULL) {
				CloseWindow(hDlg);
				hDlg = NULL;
			}
			size = sizeof(c_addr);
			cs = accept(s, (LPSOCKADDR)&c_addr, &size);
			WSAAsyncSelect(cs, hWnd, WM_ASYNC, FD_READ);
			init();
			InvalidateRect(hWnd, &board_R, TRUE);
			send(cs, (LPSTR)"ok", 3, 0);
			break;
		case FD_READ:
			isInit = true;
			if (hDlg != NULL) {
				CloseWindow(hDlg);
				hDlg = NULL;
			}
			if (isServer)
				msgLen = recv(cs, buffer, 100, 0);
			else
				msgLen = recv(s, buffer, 100, 0);
			buffer[msgLen] = NULL;
#ifdef _UNICODE
			msgLen = MultiByteToWideChar(CP_ACP, 0, buffer, strlen(buffer), NULL, NULL);
			MultiByteToWideChar(CP_ACP, 0, buffer, strlen(buffer), msg, msgLen);
			msg[msgLen] = NULL;
#else
			strcpy_s(msg, buffer);
#endif
			if (!(isServer||strcmp(buffer, "ok"))) {
				init();
				InvalidateRect(hWnd, &board_R, TRUE);
			}
			else {
				POINT tmp1;
				switch (buffer[0]) {
				case 'M':
					tmp.x = '7' - buffer[2];
					tmp.y = '7' - buffer[3];
					tmp1.x = '7' - buffer[5];
					tmp1.y = '7' - buffer[6];
					Players[1].move(board, tmp, tmp1);
					isTurn = true;
					InvalidateRect(hWnd, &board_R, TRUE);
					break;
				case 'A':
					tmp.x = '7' - buffer[2];
					tmp.y = '7' - buffer[3];
					if (Players[0].attacked(tmp))
						isLose = true;
					board[tmp.y][tmp.x] = NONE;
					break;
				case 'Q':
					isTurn = false;
					if (MessageBox(hWnd, _T("축하합니다!! 승리하였습니다. 한판더 하시겠습니까?"), _T("Game Over"), MB_YESNO) == IDYES)
						Restart();
					else
						PostQuitMessage(0);
					break;
				case 'P':
					tmp.x = '7' - buffer[2];
					tmp.y = '7' - buffer[3];
					tmp1.x = '7' - buffer[5];
					tmp1.y = '7' - buffer[6];
					if (Players[0].attacked(tmp)) {
						isLose = true;
					}
					int kind = buffer[8] * 10 + buffer[9] - '0' * 11;
					Players[1].setRef(tmp);
					Players[1].promotion(board, tmp1, kind);
					InvalidateRect(hWnd, &board_R, TRUE);
					isTurn = true;
					break;
				}
			}
			break;
		default:
			break;
		}
		return 0;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		if (!isInit&&hDlg==NULL) {
			hDlg = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_DIALOG3), hWnd, DlgProc3);
			ShowWindow(hDlg,SW_SHOW);
		//	return 0;
		}
		color = RGB(128, 128, 0);
		hBrush = CreateSolidBrush(color);
		hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);
		
		Rectangle(hdc, 0, 0, 600, 600);
		SelectObject(hdc, hOldBrush);
		DeleteObject(hBrush);

		for (i = 0;i < 8;i++)
			for (j = 0;j < 8;j++)
			{
				if (board[j][i] == CATCH + MY_KING)
					board[j][i] -= CATCH;

				if (board[j][i] == CAN_MOVE)
					color = RGB(255, 255, 0);
				else if (board[j][i] == CASTLING)
					color = RGB(0, 255, 0);
				else if (board[j][i] >= ENPASSANT)
					color = RGB(0, 255, 255);
				else if (board[j][i] > CATCH)
					color = RGB(255, 0, 0);
				else if (board[j][i] >= PROMOTION)
					color = RGB(0, 0, 255);
				else {
					if (i % 2 == j % 2)
						color = RGB(255, 255, 255);
					else
						color = RGB(0, 0, 0);
				}
				hBrush = CreateSolidBrush(color);
				hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);
				Rectangle(hdc, 70 * i + 20, 70 * j + 20, 70 * (i + 1) + 20, 70 * (j + 1) + 20);

				SelectObject(hdc, hOldBrush);
				DeleteObject(hBrush);
				if (board[j][i] > 10) {
					if (board[j][i] < 20)
						hBit = AllPieces[0][board[j][i] - 11];
					else
						hBit = AllPieces[1][board[j][i] - 21];
					MemDC = CreateCompatibleDC(hdc);

					OldBitmap = (HBITMAP)SelectObject(MemDC, hBit);
					GetObject(hBit, sizeof(BITMAP), &bit);
					StretchBlt(hdc, 70 * i + 25, 70 * j + 25, 60, 60, MemDC, 0, 0, bit.bmWidth, bit.bmHeight, SRCCOPY);

					SelectObject(MemDC, OldBitmap);
					DeleteDC(MemDC);
				}

				moveP[i][j] = board[i][j];
			}
		if (isInit&&!isLose) {
			color = RGB(255, 0, 255);
			hBrush = CreateSolidBrush(color);
			hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);
			Players[1].show(moveP);
			POINT kp;
			if (Players[0].isCheck(moveP)) {
				isCheck = true;
				kp = Players[0].getKingPos();
				Rectangle(hdc, 70 * kp.x + 20, 70 * kp.y + 20, 70 * (kp.x + 1) + 20, 70 * (kp.y + 1) + 20);
			}
			for (i = 0;i < 8;i++)
				for (j = 0;j < 8;j++)
					moveP[i][j] = board[i][j];
			Players[0].show(moveP);
			if (Players[1].isCheck(moveP)) {
				kp = Players[1].getKingPos();
				Rectangle(hdc, 70 * kp.x + 20, 70 * kp.y + 20, 70 * (kp.x + 1) + 20, 70 * (kp.y + 1) + 20);
			}
			for (i = 0;i < 8;i++)
				for (j = 0;j < 8;j++)
					moveP[i][j] = board[i][j];
			SelectObject(hdc, hOldBrush);
			DeleteObject(hBrush);
		}

		
		EndPaint(hWnd, &ps);
		if (isCheck&&isWarn) {
			MessageBox(hWnd, _T("체크당하셨습니다. 주의하세요!!"), _T("주의하세요"), MB_OK | MB_ICONWARNING);
			isWarn = false;
		}if (isLose) {
			isLose = isTurn = false;
			if (MessageBox(hWnd, _T("아쉽습니다... 게임에서 패배하였습니다. 한판더 하시겠습니까?"), _T("Game Over"), MB_YESNO) == IDYES) {
				Restart();
			}
			else
				PostQuitMessage(0);
			return 0;
		}
		return 0;
	case WM_LBUTTONDOWN:
		tmp.x = LOWORD(lParam);
		tmp.y = HIWORD(lParam);

		if (isTurn&&PtInRect(&board_R, tmp)) {
			tmp.x = (tmp.x - 20) / 70;
			tmp.y = (tmp.y - 20) / 70;
			if (isCanMove) {
				isWarn = true;
				if (board[tmp.y][tmp.x] >= ENPASSANT) {
					tmp.y++;
					if (Players[1].attacked(tmp)) {
						isWin = true;
					}
					sprintf(buffer, "A %d%d", tmp.x, tmp.y);
					board[tmp.y][tmp.x] = NONE;
					if (isServer) {
						send(cs, (LPSTR)buffer, strlen(buffer) + 1, 0);
					}
					else {
						send(s, (LPSTR)buffer, strlen(buffer) + 1, 0);
					}
					isTurn = false;
					Sleep(100);
					tmp.y--;
				}
				else if (board[tmp.y][tmp.x] > CATCH) {
					sprintf(buffer, "A %d%d", tmp.x, tmp.y);
					if (isServer) {
						send(cs, (LPSTR)buffer, strlen(buffer) + 1, 0);
					}
					else {
						send(s, (LPSTR)buffer, strlen(buffer) + 1, 0);
					}
					isTurn = false;
					Sleep(100);
					if (Players[1].attacked(tmp)) {
						isWin = true;
					}
				}
				else if (board[tmp.y][tmp.x] == CASTLING) {
					isTurn = false;
					if (isServer)
						Players[0].castling(board, tmp, cs);
					else
						Players[0].castling(board, tmp, s);
				}
				else if (board[tmp.y][tmp.x] >= PROMOTION) {
					isTurn = false;
					if (board[tmp.y][tmp.x] > PROMOTION) {
						if (Players[1].attacked(tmp)) {
							isWin = true;
						}
					}
					poin = tmp;
					DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DIALOG1), hWnd, DlgProc1);
			
				}
				if (Players[0].move(board, tmp)) {
					sprintf(buffer, "M %d%d %d%d", bpoint.x, bpoint.y, tmp.x, tmp.y);
					if (isServer) {
						send(cs, (LPSTR)buffer, strlen(buffer)+1, 0);
					}
					else {
						send(s, (LPSTR)buffer, strlen(buffer)+1, 0);
					}
					if (isWin) {
						if (MessageBox(hWnd, _T("축하합니다!! 승리하였습니다. 한판더 하시겠습니까?"), _T("Game Over"), MB_YESNO) == IDYES) {
							Restart();
						}
						else
							PostQuitMessage(0);
					}
					isWin = isTurn = false;
				}
			}
			else {
				Players[1].show(moveP);
				Players[1].can_enpassant(board,tmp, isServer);
				
				Players[0].show(board,moveP,tmp);
				bpoint.x = tmp.x;
				bpoint.y = tmp.y;
			}
			InvalidateRect(hWnd, &board_R, TRUE);
			isCanMove = !isCanMove;

		}
		return 0;
	case WM_DESTROY:
		sprintf(buffer, "Q");
		if (isServer) {
			send(cs, (LPSTR)buffer, strlen(buffer) + 1, 0);
		}
		else {
			send(s, (LPSTR)buffer, strlen(buffer) + 1, 0);
		}
		PostQuitMessage(0);
		return 0;
	}
	return(DefWindowProc(hWnd, iMessage, wParam, lParam));
}

BOOL CALLBACK DlgProc1(HWND hDlg, UINT iMessage, WPARAM wParam, LPARAM lParam) {
	char buffer[100];
	switch (iMessage) {
	case WM_INITDIALOG:
		CheckRadioButton(hDlg, IDC_QUEEN, IDC_KNIGHT, IDC_QUEEN);
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			if (IsDlgButtonChecked(hDlg, IDC_QUEEN) == BST_CHECKED) {
				Players[0].promotion(board, poin, MY_QUEEN);
				sprintf(buffer, "P %d%d %d%d %d", bpoint.x,bpoint.y,poin.x, poin.y, NMY_QUEEN);
			}
			else if (IsDlgButtonChecked(hDlg, IDC_ROOK) == BST_CHECKED) {
				Players[0].promotion(board, poin, MY_ROOK);
				sprintf(buffer, "P %d%d %d%d %d", bpoint.x, bpoint.y, poin.x, poin.y, NMY_ROOK);
			}
			else if (IsDlgButtonChecked(hDlg, IDC_BISHOP) == BST_CHECKED) {
				Players[0].promotion(board, poin, MY_BISHOP);
				sprintf(buffer, "P %d%d %d%d %d", bpoint.x, bpoint.y, poin.x, poin.y, NMY_BISHOP);
			}
			else if (IsDlgButtonChecked(hDlg, IDC_KNIGHT) == BST_CHECKED) {
				Players[0].promotion(board, poin, MY_KNIGHT);
				sprintf(buffer, "P %d%d %d%d %d", bpoint.x, bpoint.y, poin.x, poin.y, NMY_KNIGHT);
			}
			if (isServer) {
				send(cs, (LPSTR)buffer, strlen(buffer) + 1, 0);
			}
			else {
				send(s, (LPSTR)buffer, strlen(buffer) + 1, 0);
			}
			EndDialog(hDlg, IDOK);
			break;
		}
		return TRUE;
	}
	return FALSE;
}

BOOL CALLBACK DlgProc2(HWND hDlg, UINT iMessage, WPARAM wParam, LPARAM lParam) {
	static HWND hIp;
	switch (iMessage) {
	case WM_INITDIALOG:
		hIp = GetDlgItem(hDlg, IDC_IPADDRESS1);
		CheckRadioButton(hDlg, IDC_SERVER, IDC_CLIENT, IDC_SERVER);
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			if (IsDlgButtonChecked(hDlg, IDC_SERVER) == BST_CHECKED) {
				isServer = true;
				IPv4 ip;
				getMyIP(ip);
				_stprintf(ServerIP, _T("%d.%d.%d.%d"), ip.b1,ip.b2,ip.b3,ip.b4);
			}
			else {
				DWORD dwAddr;
				SendMessage(hIp, IPM_GETADDRESS, 0, (LPARAM)&dwAddr);
				if (dwAddr == 0) {
					MessageBox(hDlg,_T("정확한 IP를 적어주세요"),_T("에러"),MB_OK|MB_ICONERROR);
					return TRUE;
				}
				_stprintf(ServerIP,  _T("%d.%d.%d.%d"),
					FIRST_IPADDRESS(dwAddr),
					SECOND_IPADDRESS(dwAddr),
					THIRD_IPADDRESS(dwAddr),
					FOURTH_IPADDRESS(dwAddr));
				isServer = false;
			}
			EndDialog(hDlg, IDOK);
			break;
		case IDC_CLIENT:
			ShowWindow(hIp, SW_SHOW);
			break;
		case IDC_SERVER:
			ShowWindow(hIp, SW_HIDE);
			break;
		}
		return TRUE;
	case WM_SYSCOMMAND:
		switch (wParam)
		{
		case SC_MINIMIZE:
			//------------------------------------------------------
			break;
		case SC_CLOSE:
			//------------------------------------------------------
			PostQuitMessage(0);
			break;
		default:
			return FALSE;
		}
		return TRUE;


	}
	return FALSE;
}
BOOL CALLBACK DlgProc3(HWND hDlg, UINT iMessage, WPARAM wParam, LPARAM lParam) {
	TCHAR str[100];
	switch (iMessage) {
	case WM_INITDIALOG:
		HFONT hFont;
		hFont = CreateFont(20, 0, 0, 0, 0, 0, 0, 0, HANGEUL_CHARSET, 0, 0, 0, VARIABLE_PITCH | FF_ROMAN, _T("고딕"));

		SendMessage(GetDlgItem(hDlg, IDC_STATIC1), WM_SETFONT, (WPARAM)hFont, 0);
		if (isServer) {
			_tcscpy(str, _T("클라이언트에게 당신의 IP("));
			_tcscat(str, ServerIP);
			_tcscat(str, _T(")를 알려주세요."));
		}
		else {
			_tcscpy(str, _T("이 글을 보신다면 서버의 IP("));
			_tcscat(str, ServerIP);
			_tcscat(str, _T(")를 다시한번 확인하세요."));

		}
		SetDlgItemText(hDlg, IDC_STATIC1,str);

			
		DeleteObject(hFont);
		return TRUE;
	case WM_SYSCOMMAND:
		switch (wParam)
		{
		case SC_MINIMIZE:
			//------------------------------------------------------
			break;
		case SC_CLOSE:
			//------------------------------------------------------
			PostQuitMessage(0);
			break;
		default:
			return FALSE;
		}
		return TRUE;
	}
	return FALSE;

}

void init() {
	int k,i,j;
	Players[0].init(true, isServer);
	Players[1].init(false, isServer);
	isTurn = isServer;
	k = !isServer;
	for (i = 0;i < 2;i++) {
		for (j = 0;j < 6;j++)
			AllPieces[i][j] = LoadBitmap(g_hInst, MAKEINTRESOURCE(IDB_BITMAP1 + j + k * 6));
		k = !k;
	}
	board[0][0] = board[0][7] = NMY_ROOK;
	board[0][1] = board[0][6] = NMY_KNIGHT;
	board[0][2] = board[0][5] = NMY_BISHOP;
	board[0][3] = isServer ? NMY_QUEEN : NMY_KING;
	board[0][4] = isServer ? NMY_KING : NMY_QUEEN;

	for (i = 2;i < 6;i++)
		for (j = 0;j < 8;j++)
			board[i][j] = NONE;

	for (i = 0;i < 8;i++) {
		board[6][i] = MY_PAWN;
		board[1][i] = NMY_PAWN;
	}

	board[7][0] = board[7][7] = MY_ROOK;
	board[7][1] = board[7][6] = MY_KNIGHT;
	board[7][2] = board[7][5] = MY_BISHOP;
	board[7][3] = isServer ? MY_QUEEN : MY_KING;
	board[7][4] = isServer ? MY_KING : MY_QUEEN;
}

bool getMyIP(IPv4 & myIP)
{
	char szBuffer[1024];

#ifdef WIN32
	WSADATA wsaData;
	WORD wVersionRequested = MAKEWORD(2, 0);
	if (::WSAStartup(wVersionRequested, &wsaData) != 0)
		return false;
#endif


	if (gethostname(szBuffer, sizeof(szBuffer)) == SOCKET_ERROR)
	{
#ifdef WIN32
		WSACleanup();
#endif
		return false;
	}

	struct hostent *host = gethostbyname(szBuffer);
	if (host == NULL)
	{
#ifdef WIN32
		WSACleanup();
#endif
		return false;
	}

	//Obtain the computer's IP
	myIP.b1 = ((struct in_addr *)(host->h_addr))->S_un.S_un_b.s_b1;
	myIP.b2 = ((struct in_addr *)(host->h_addr))->S_un.S_un_b.s_b2;
	myIP.b3 = ((struct in_addr *)(host->h_addr))->S_un.S_un_b.s_b3;
	myIP.b4 = ((struct in_addr *)(host->h_addr))->S_un.S_un_b.s_b4;

#ifdef WIN32
	WSACleanup();
#endif
	return true;
}

void Restart() {
	HWND hTempWnd = hWndMain;
	TCHAR szFileName[MAX_PATH] = _T("");

	GetModuleFileName(NULL, szFileName, MAX_PATH);
	ShellExecute(GetDesktopWindow(), _T("OPEN"), szFileName, NULL, NULL, SW_SHOWDEFAULT);
	SendMessage(hTempWnd, WM_DESTROY, 0, 0);
}