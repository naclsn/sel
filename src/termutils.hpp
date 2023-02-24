#ifndef SELI_TERMUTILS_HPP
#define SELI_TERMUTILS_HPP

#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN
# define VC_EXTRALEAN
# include <Windows.h>
#else
# include <cstdio>
# include <sys/ioctl.h>
#endif

int term_size(unsigned& width, unsigned& height) {
#ifdef _WIN32
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  bool a = GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
  if (!a) return -1;
  width = csbi.srWindow.Right - csbi.srWindow.Left+1;
  height = csbi.srWindow.Bottom - csbi.srWindow.Top+1;
#elif defined TIOCGSIZE
  struct ttysize ts;
  int a = ioctl(fileno(stdout), TIOCGSIZE, &ts);
  if (0 != a) return a;
  width = ts.ts_cols;
  height = ts.ts_lines;
#elif defined(TIOCGWINSZ)
  struct winsize ts;
  int a = ioctl(fileno(stdout), TIOCGWINSZ, &ts);
  if (0 != a) return a;
  width = ts.ws_col;
  height = ts.ws_row;
#endif
  return 0;
}

#endif // SELI_TERMUTILS_HPP
