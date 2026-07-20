// Clawdmeter daemon launcher — the main executable of ClawdmeterDaemon.app.
//
// Why this exists: macOS attributes Bluetooth (TCC) access to the "responsible
// process". A bare CommandLineTools python3 launched by launchd has no bundle
// identity, can't show a permission prompt, and gets silently denied. By making
// the app's main executable a compiled Mach-O that lives inside the bundle and
// stays alive as the parent of python, CoreBluetooth attributes the request to
// THIS bundle ("Clawdmeter Daemon") — a stable, named entry in
// System Settings > Privacy & Security > Bluetooth.
//
// We fork+exec (not exec) so this in-bundle binary remains the live parent;
// python inherits its TCC responsibility. We forward SIGTERM/SIGINT so launchd
// stop/reload shuts the daemon down cleanly instead of orphaning it.
//
// Invoked as:  clawdmeter-daemon <python> <daemon.py> [extra args...]

#include <errno.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

static volatile pid_t child = 0;

static void forward(int sig) {
    if (child > 0) {
        kill(child, sig);
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <python> <daemon.py> [args...]\n", argv[0]);
        return 2;
    }

    // Child argv = everything after our own executable: python daemon.py ...
    char **child_argv = &argv[1];

    signal(SIGTERM, forward);
    signal(SIGINT, forward);

    pid_t spawned = 0;
    int rc = posix_spawn(&spawned, child_argv[0], NULL, NULL, child_argv, environ);
    child = spawned;
    if (rc != 0) {
        fprintf(stderr, "clawdmeter-daemon: posix_spawn(%s) failed: %s\n",
                child_argv[0], strerror(rc));
        return 1;
    }

    int status = 0;
    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR) {
            perror("clawdmeter-daemon: waitpid");
            return 1;
        }
        // EINTR: our signal handler fired; loop and reap the child.
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return 0;
}
