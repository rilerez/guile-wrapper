#include <libguile.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "scm.hpp"
#include "subr.hpp"

using namespace guile;
using namespace std;

namespace {
// const int WIDTH = 10;
// const int HEIGHT = 10;

// FILE* start_gnuplot() {
//   FILE* output;
//   // All data traveling through the pipe moves through the kernel
//   int pipes[2];
//   pid_t pid;

//   pipe(pipes);
//   pid = fork();
//   if(pid > 0) {
//     /* Parent process closes up input side of pipe */
//     close(pipes[0]);

//     output = fdopen(pipes[1], "w");

//     fprintf(output, "set multiplot\n");
//     fprintf(output, "set parametric\n");
//     fprintf(output, "set xrange [-%d:%d]\n", WIDTH, WIDTH);
//     fprintf(output, "set yrange [-%d:%d]\n", HEIGHT, HEIGHT);
//     fprintf(output, "set size ratio -1\n");
//     fprintf(output, "unset xtics\n");
//     fprintf(output, "unset ytics\n");
//     fflush(output);

//     return output;
//   } else if(pid == 0) {
//     /*CHILD*/
//     /* Child process closes up output side of pipe */
//     close(pipes[1]);

//     dup2(pipes[0], STDIN_FILENO);

//     execlp("gnuplot", "");
//     return nullptr; /* Not reached.  */
//   } else {
//     perror("fork");
//     exit(1);
//   }
// }

FILE* global_output;

void draw_line(FILE* output, double x1, double y1, double x2, double y2) {
  fprintf(output, "plot [0:1] %f + %f * t, %f + %f * t notitle\n", x1, x2 - x1,
          y1, y2 - y1);
  fflush(output);
}

double x, y;
double direction;
bool pendown;

GUILE_DEF_SUBR(tortoise_reset, "tortoise-reset", (), (), (), {
  x = 0.0;
  y = 0.0;
  direction = 0.0;
  pendown = true;

  fprintf(global_output, "clear\n");
  fflush(global_output);
})

GUILE_DEF_SUBR(tortoise_pendown, "tortoise-pendown", (), (), (), {
  bool result = pendown;
  pendown = true;
  return result;
})

GUILE_DEF_SUBR(tortoise_penup, "tortoise-penup", (), (), (), {
  bool result = pendown;
  pendown = false;
  return result;
})

GUILE_DEF_SUBR(tortoise_turn, "tortoise-turn", (const double degrees), (), (), {
  direction += M_PI / 180.0 * degrees;
  return scm{direction * 180.0 / M_PI};
})

GUILE_DEF_SUBR(tortoise_move, "tortoise-move", (const double length), (), (), {
  const double newX = x + length * cos(direction);
  const double newY = y + length * sin(direction);

  if(pendown) draw_line(global_output, x, y, newX, newY);
  x = newX;
  y = newY;

  return list(x, y);
})
}  // namespace

int main(int argc, char* argv[]) {
  global_output = fdopen(STDOUT_FILENO, "w");

  with_guile([] {
#define DEFIT(name) definer::tortoise_##name()
    DEFIT(reset);
    DEFIT(penup);
    DEFIT(pendown);
    DEFIT(turn);
    DEFIT(move);
#undef DEFIT
  });
  tortoise_reset();
  scm_shell(argc, argv);

  return EXIT_SUCCESS;
}
