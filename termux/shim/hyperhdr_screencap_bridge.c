/*
 * HyperHDR Native ADB Screencap Bridge for Android TV
 *
 * Runs as UID 2000 (shell) on Android TV to capture the screen via
 * /system/bin/screencap, downscales frames in RAM, and streams them
 * via UDP loopback (:19446) directly to HyperHDR in Termux.
 *
 * Zero APK installation required.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define DST_W 160
#define DST_H 90
#define PACKET_HDR_LEN 8
#define PACKET_SIZE (PACKET_HDR_LEN + DST_W * DST_H * 3)
#define PID_FILE "/data/local/tmp/screencap_bridge.pid"
#define LOG_FILE "/data/local/tmp/screencap_bridge.log"

static volatile sig_atomic_t g_running = 1;

static void handle_signal(int sig) {
    (void)sig;
    g_running = 0;
}

static double get_monotonic_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

static void print_usage(const char *prog) {
    printf("HyperHDR Screencap Bridge (Native Android Shell Grabber)\n");
    printf("Usage: %s [options]\n", prog);
    printf("  -d, --daemon     Run in background as daemon\n");
    printf("  -k, --stop       Stop running daemon\n");
    printf("  -s, --status     Check running daemon status\n");
    printf("  --fps <n>        Target framerate (default: 3, max: 10)\n");
    printf("  -h, --help       Show this help message\n");
}

static int stop_daemon(void) {
    FILE *fp = fopen(PID_FILE, "r");
    if (!fp) {
        printf("Bridge daemon is not running (no PID file).\n");
        return 0;
    }
    pid_t pid = 0;
    if (fscanf(fp, "%d", &pid) != 1 || pid <= 0) {
        fclose(fp);
        unlink(PID_FILE);
        printf("Bridge daemon is not running (invalid PID).\n");
        return 0;
    }
    fclose(fp);

    if (kill(pid, 0) == 0) {
        kill(pid, SIGTERM);
        printf("Sent SIGTERM to bridge daemon (PID %d).\n", pid);
        for (int i = 0; i < 20; i++) {
            usleep(100000);
            if (kill(pid, 0) != 0) break;
        }
        unlink(PID_FILE);
        printf("Bridge daemon stopped.\n");
    } else {
        unlink(PID_FILE);
        printf("Bridge daemon was not running (stale PID %d removed).\n", pid);
    }
    return 0;
}

static int check_status(void) {
    FILE *fp = fopen(PID_FILE, "r");
    if (!fp) {
        printf("Bridge daemon: STOPPED\n");
        return 1;
    }
    pid_t pid = 0;
    if (fscanf(fp, "%d", &pid) != 1 || pid <= 0) {
        fclose(fp);
        unlink(PID_FILE);
        printf("Bridge daemon: STOPPED\n");
        return 1;
    }
    fclose(fp);

    if (kill(pid, 0) == 0) {
        printf("Bridge daemon: RUNNING (PID %d)\n", pid);
        return 0;
    } else {
        unlink(PID_FILE);
        printf("Bridge daemon: STOPPED (cleaned up stale PID %d)\n", pid);
        return 1;
    }
}

int main(int argc, char *argv[]) {
    int daemon_mode = 0;
    int target_fps = 3;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--daemon") == 0) {
            daemon_mode = 1;
        } else if (strcmp(argv[i], "-k") == 0 || strcmp(argv[i], "--stop") == 0) {
            return stop_daemon();
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--status") == 0) {
            return check_status();
        } else if (strcmp(argv[i], "--fps") == 0 && i + 1 < argc) {
            target_fps = atoi(argv[++i]);
            if (target_fps < 1) target_fps = 1;
            if (target_fps > 15) target_fps = 15;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    // Stop existing instance if already running
    FILE *pfp = fopen(PID_FILE, "r");
    if (pfp) {
        pid_t existing_pid = 0;
        if (fscanf(pfp, "%d", &existing_pid) == 1 && existing_pid > 0 && kill(existing_pid, 0) == 0) {
            printf("Bridge daemon is already running (PID %d). Use --stop first.\n", existing_pid);
            fclose(pfp);
            return 0;
        }
        fclose(pfp);
    }

    if (daemon_mode) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return 1;
        }
        if (pid > 0) {
            printf("HyperHDR Screencap Bridge started in background (PID %d).\n", pid);
            return 0;
        }
        setsid();
        umask(0);
        int log_fd = open(LOG_FILE, O_WRONLY | O_CREAT | O_APPEND, 0666);
        if (log_fd >= 0) {
            dup2(log_fd, STDOUT_FILENO);
            dup2(log_fd, STDERR_FILENO);
            close(log_fd);
        }
        int devnull = open("/dev/null", O_RDONLY);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            close(devnull);
        }
    }

    FILE *pid_out = fopen(PID_FILE, "w");
    if (pid_out) {
        fprintf(pid_out, "%d\n", getpid());
        fclose(pid_out);
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        unlink(PID_FILE);
        return 1;
    }

    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(19446);
    inet_pton(AF_INET, "127.0.0.1", &dest_addr.sin_addr);

    uint8_t packet[PACKET_SIZE];
    packet[0] = 'H';
    packet[1] = 'H';
    packet[2] = 'M';
    packet[3] = 'P';
    packet[4] = (DST_W >> 8) & 0xFF;
    packet[5] = DST_W & 0xFF;
    packet[6] = (DST_H >> 8) & 0xFF;
    packet[7] = DST_H & 0xFF;

    uint8_t *raw_buf = NULL;
    size_t raw_buf_capacity = 0;

    int min_interval_ms = 1000 / target_fps;
    unsigned long frame_count = 0;
    double window_start = get_monotonic_ms();

    printf("[BRIDGE] Started screencap loop (target FPS: %d, UDP port: 19446)\n", target_fps);
    fflush(stdout);

    while (g_running) {
        double loop_t0 = get_monotonic_ms();

        FILE *fp = popen("/system/bin/screencap", "r");
        if (!fp) {
            usleep(250000);
            continue;
        }

        uint32_t hdr[4];
        if (fread(hdr, 1, 16, fp) != 16) {
            pclose(fp);
            usleep(250000);
            continue;
        }

        uint32_t src_w = hdr[0];
        uint32_t src_h = hdr[1];
        if (src_w == 0 || src_h == 0 || src_w > 7680 || src_h > 4320) {
            pclose(fp);
            usleep(250000);
            continue;
        }

        size_t needed_bytes = (size_t)src_w * src_h * 4;
        if (needed_bytes > raw_buf_capacity) {
            free(raw_buf);
            raw_buf = (uint8_t *)malloc(needed_bytes);
            if (!raw_buf) {
                raw_buf_capacity = 0;
                pclose(fp);
                usleep(250000);
                continue;
            }
            raw_buf_capacity = needed_bytes;
        }

        size_t read_bytes = 0;
        while (read_bytes < needed_bytes) {
            size_t n = fread(raw_buf + read_bytes, 1, needed_bytes - read_bytes, fp);
            if (n == 0) break;
            read_bytes += n;
        }
        pclose(fp);

        if (read_bytes >= needed_bytes) {
            // Downscale to 160x90 RGB24
            int out_idx = PACKET_HDR_LEN;
            for (int dy = 0; dy < DST_H; dy++) {
                int sy = (dy * src_h) / DST_H;
                size_t row_base = (size_t)sy * src_w * 4;
                for (int dx = 0; dx < DST_W; dx++) {
                    int sx = (dx * src_w) / DST_W;
                    size_t p = row_base + sx * 4;
                    packet[out_idx++] = raw_buf[p];     // R
                    packet[out_idx++] = raw_buf[p + 1]; // G
                    packet[out_idx++] = raw_buf[p + 2]; // B
                }
            }

            sendto(sock, packet, PACKET_SIZE, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            frame_count++;

            double now = get_monotonic_ms();
            if (now - window_start >= 5000.0) {
                double actual_fps = (frame_count * 1000.0) / (now - window_start);
                printf("[BRIDGE] Streaming active: %.2f FPS (%lu frames sent)\n", actual_fps, frame_count);
                fflush(stdout);
                frame_count = 0;
                window_start = now;
            }
        }

        double elapsed = get_monotonic_ms() - loop_t0;
        int sleep_ms = min_interval_ms - (int)elapsed;
        if (sleep_ms > 10) {
            usleep(sleep_ms * 1000);
        } else {
            usleep(5000); // Yield CPU
        }
    }

    printf("[BRIDGE] Stopping screencap loop...\n");
    if (raw_buf) free(raw_buf);
    close(sock);
    unlink(PID_FILE);
    printf("[BRIDGE] Stopped cleanly.\n");
    return 0;
}
