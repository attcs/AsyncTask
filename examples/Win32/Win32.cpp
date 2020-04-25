// Win32.cpp : Defines the entry point for the application.
//
#define _ENABLE_ATOMIC_ALIGNMENT_FIX 1

#include "framework.h"
#include "Win32.h"
#include "commctrl.h"
#include <assert.h>
#include <memory>
#include <tuple>
#include <string>


#include <exception>
#include <future>
#include <atomic>
#include "../../asynctask.h"

#include "Calculation.h"

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    DlgCalcProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
  _In_opt_ HINSTANCE hPrevInstance,
  _In_ LPWSTR    lpCmdLine,
  _In_ int       nCmdShow)
{
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);

  // TODO: Place code here.

  // Initialize global strings
  LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
  LoadStringW(hInstance, IDC_WIN32, szWindowClass, MAX_LOADSTRING);
  MyRegisterClass(hInstance);

  // Perform application initialization:
  if (!InitInstance(hInstance, nCmdShow))
  {
    return FALSE;
  }

  HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WIN32));

  MSG msg;

  // Main message loop:
  while (GetMessage(&msg, nullptr, 0, 0))
  {
    if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
    {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  return (int)msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
  WNDCLASSEXW wcex;

  wcex.cbSize = sizeof(WNDCLASSEX);

  wcex.style = CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc = WndProc;
  wcex.cbClsExtra = 0;
  wcex.cbWndExtra = 0;
  wcex.hInstance = hInstance;
  wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WIN32));
  wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_WIN32);
  wcex.lpszClassName = szWindowClass;
  wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

  return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
  hInst = hInstance; // Store instance handle in our global variable

  HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
    CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

  if (!hWnd)
  {
    return FALSE;
  }

  ShowWindow(hWnd, nCmdShow);
  UpdateWindow(hWnd);

  return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
  {
    case WM_COMMAND:
    {
      int wmId = LOWORD(wParam);
      // Parse the menu selections:
      switch (wmId)
      {
        case IDM_ABOUT:
          DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
          break;
        case ID_CALCULATION_CALCULATION:
          DialogBox(hInst, MAKEINTRESOURCE(IDD_CALC), hWnd, DlgCalcProc);
          break;
        case IDM_EXIT:
          DestroyWindow(hWnd);
          break;
        default:
          return DefWindowProc(hWnd, message, wParam, lParam);
      }
    }
    break;
    case WM_PAINT:
    {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hWnd, &ps);
      // TODO: Add any drawing code that uses hdc here...
      EndPaint(hWnd, &ps);
    }
    break;
    case WM_DESTROY:
      PostQuitMessage(0);
      break;
    default:
      return DefWindowProc(hWnd, message, wParam, lParam);
  }
  return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
  UNREFERENCED_PARAMETER(lParam);
  switch (message)
  {
    case WM_INITDIALOG:
      return (INT_PTR)TRUE;

    case WM_COMMAND:
      if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
      {
        EndDialog(hDlg, LOWORD(wParam));
        return (INT_PTR)TRUE;
      }
      break;
  }
  return (INT_PTR)FALSE;
}


namespace
{
  constexpr size_t N = 1000;
  struct Progress { int iP = 0; LPCWSTR szFeedback = L""; };
  using Result = Matrix;

  class AsyncCalculation : public AsyncTask<Progress, Result, Matrix, Matrix>
  {
  public:
    HWND hDlg = NULL;

  protected:

    Result doInBackground(Matrix const& m1, Matrix const& m2) override
    {
      auto const n = m1.size();
      auto m = Result(N);
      auto p = Progress();
      for (size_t i = 0; i < n; ++i)
      {
        m[i].resize(N);
        for (size_t j = 0; j < n; ++j)
        {
          m[i][j] = matrix_product_element(m1, m2, i, j);
          if (isCancelled())
            break;
        }
        
        if (i == 100)
          p.szFeedback = L"Async thread report: Over the 100th row!";
        else if (i == 500)
          p.szFeedback = L"Async thread report: Over the 500th row!";
        else if (i == 900)
          p.szFeedback = L"Async thread report: Almost finish!";
          
        p.iP = static_cast<int>(i);
        publishProgress(p);
      }
      return m;
    }

    void onPreExecute() override 
    {
      assert(hDlg != NULL);

      SendDlgItemMessage(hDlg, IDC_PROGRESS1, PBM_SETRANGE, 0, MAKELPARAM(0, N));
      SendDlgItemMessage(hDlg, IDC_PROGRESS1, PBM_SETSTEP, (WPARAM)1, 0);

      SendDlgItemMessage(hDlg, IDC_LIST1, LB_ADDSTRING, 0, (LPARAM)L"Calculation is began.");
    }
    
  private:
    LPCWSTR szFeedbackLatest = L"";

  public:
    void onProgressUpdate(Progress const& progress) override
    {
      SendDlgItemMessage(hDlg, IDC_PROGRESS1, PBM_SETPOS, (WPARAM)progress.iP, 0);
      
      if (progress.szFeedback != szFeedbackLatest)
      {
        SendDlgItemMessage(hDlg, IDC_LIST1, LB_ADDSTRING, 0, (LPARAM)progress.szFeedback);
        szFeedbackLatest = progress.szFeedback;
      }
    }

    void onPostExecute(Result const& result) override 
    {
      SendDlgItemMessage(hDlg, IDC_PROGRESS1, PBM_SETSTEP, N, 0);
      SendDlgItemMessage(hDlg, IDC_LIST1, LB_ADDSTRING, 0, (LPARAM)L"Calculation is finished properly.");
      EnableWindow(GetDlgItem(hDlg, IDC_BUTTON1), FALSE);
    }

    void onCancelled() override
    {
      SendDlgItemMessage(hDlg, IDC_LIST1, LB_ADDSTRING, 0, (LPARAM)L"Calculation is interrupted by cancellation.");
    }

  };
  
  std::unique_ptr<AsyncCalculation> pAsyncCalculation;
}

#define IDT_TIMER1 10000

// Message handler for Calculation dialog.
INT_PTR CALLBACK DlgCalcProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
  UNREFERENCED_PARAMETER(lParam);
  static UINT_PTR idTimer;

  switch (message)
  {
    case WM_INITDIALOG:
    {
      idTimer = SetTimer(hDlg, IDT_TIMER1, 10, NULL);
      
      auto const m1 = matrix_random(N, N);
      auto const m2 = matrix_random(N, N);

      pAsyncCalculation.reset(new AsyncCalculation());
      pAsyncCalculation->hDlg = hDlg;

      pAsyncCalculation->execute(m1, m2);

      return (INT_PTR)TRUE;
    }
    
    case WM_TIMER:
      switch (wParam)
      {
        case IDT_TIMER1:
          pAsyncCalculation->onCallbackLoop();
          return FALSE;
      }
      break;
      
    case WM_COMMAND:
      switch (LOWORD(wParam))
      {
        case IDC_BUTTON1:
          pAsyncCalculation->cancel();
          SendDlgItemMessage(hDlg, IDC_LIST1, LB_ADDSTRING, 0, (LPARAM)L"Calculation is cancelled.");
          EnableWindow(GetDlgItem(hDlg, IDC_BUTTON1), FALSE);

          break;

        case IDOK:
        case IDCANCEL:
        {
          auto const mresult = pAsyncCalculation->get();
          //TODO: save and use the mresult...

          EndDialog(hDlg, LOWORD(wParam));
          return (INT_PTR)TRUE;
        }
        default:
          break;
      }
      break;

    case WM_DESTROY:
      KillTimer(hDlg, idTimer);
      break;

  }

  return (INT_PTR)FALSE;
}
