#include "n2_console.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_console.h"
#include "esp_log.h"

#include "n2_app.h"
#include "n2_topics.h"

static const char *TAG = "n2_console";

static int cmd_status(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    N2_LOCK();
    flog_counters_t c = g_n2.flog.c;
    printf("alarme=%s debug=%d tsrc=%s evenements=%lu a_publier=%lu\n", alarm_state_name(g_n2.alarm.state),
           g_n2.dbg.enabled, timebase_src_name(g_n2.tb.src),
           (unsigned long)(g_n2.evlog.next_seq - evlog_oldest(&g_n2.evlog)),
           (unsigned long)(g_n2.evlog_ok ? evlog_pending(&g_n2.evlog) : 0));
    N2_UNLOCK();
    printf("trames=%lu ok=%lu erreurs=%lu nack=%lu crc=%lu perdues=%lu fronts_perdus=%lu\n",
           (unsigned long)c.total, (unsigned long)c.ok, (unsigned long)c.errors, (unsigned long)c.addr_nack,
           (unsigned long)c.crc_errors, (unsigned long)c.lost, (unsigned long)n2_app_edges_dropped());
    return 0;
}

static int cmd_debug(int argc, char **argv)
{
    if (argc < 2 || (strcmp(argv[1], "on") != 0 && strcmp(argv[1], "off") != 0)) {
        printf("usage : debug on [duree_s] | debug off\n");
        return 1;
    }
    uint32_t ttl = argc > 2 ? (uint32_t)strtoul(argv[2], NULL, 10) : 0;
    n2_app_local_debug(strcmp(argv[1], "on") == 0, ttl);
    return 0;
}

static int cmd_arm(int argc, char **argv)
{
    (void)argc;
    n2_app_local_arm(strcmp(argv[0], "arm") == 0, AL_SRC_LOCAL_CONSOLE);
    return 0;
}

static int cmd_events(int argc, char **argv)
{
    unsigned n = argc > 1 ? (unsigned)strtoul(argv[1], NULL, 10) : 20u;
    char buf[200];
    N2_LOCK();
    uint32_t next = g_n2.evlog.next_seq;
    uint32_t from = evlog_oldest(&g_n2.evlog);
    if (next > from + n) {
        from = next - n;
    }
    for (uint32_t s = from; g_n2.evlog_ok && s < next; s++) {
        n2_record_t r;
        if (evlog_read(&g_n2.evlog, s, &r) == 0 && n2_fmt_event(buf, sizeof(buf), &r) > 0) {
            printf("%s\n", buf);
        }
    }
    N2_UNLOCK();
    return 0;
}

static int cmd_frames(int argc, char **argv)
{
    unsigned n = argc > 1 ? (unsigned)strtoul(argv[1], NULL, 10) : 20u;
    char buf[384];
    N2_LOCK();
    uint32_t after = g_n2.flog.next_seq > n + 1u ? g_n2.flog.next_seq - n - 1u : 0u;
    flog_entry_t e;
    while (flog_get_after(&g_n2.flog, after, &e)) {
        after = e.seq;
        if (n2_fmt_frame(buf, sizeof(buf), &e, 1) > 0) {
            printf("%s\n", buf);
        }
    }
    N2_UNLOCK();
    return 0;
}

/* Effacement des preuves : action locale explicite avec mot de confirmation. */
static int cmd_events_clear(int argc, char **argv)
{
    if (argc != 2 || strcmp(argv[1], "CONFIRMER") != 0) {
        printf("usage : events-clear CONFIRMER (efface definitivement le journal d'evenements)\n");
        return 1;
    }
    int r = n2_app_local_clear_events();
    printf(r == 0 ? "journal efface\n" : "echec\n");
    return r == 0 ? 0 : 1;
}

void n2_console_start(void)
{
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t rc = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    rc.prompt = "n2>";
    esp_console_dev_usb_serial_jtag_config_t hc = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    if (esp_console_new_repl_usb_serial_jtag(&hc, &rc, &repl) != ESP_OK) {
        ESP_LOGW(TAG, "console indisponible");
        return;
    }
    const esp_console_cmd_t cmds[] = {
        { .command = "status", .help = "etat et compteurs", .func = cmd_status },
        { .command = "debug", .help = "debug on [duree_s] | debug off", .func = cmd_debug },
        { .command = "arm", .help = "armer l'alarme (temporisation de sortie)", .func = cmd_arm },
        { .command = "disarm", .help = "desarmer l'alarme", .func = cmd_arm },
        { .command = "events", .help = "events [n] : derniers evenements", .func = cmd_events },
        { .command = "frames", .help = "frames [n] : dernieres trames I2C", .func = cmd_frames },
        { .command = "events-clear", .help = "events-clear CONFIRMER", .func = cmd_events_clear },
    };
    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        esp_console_cmd_register(&cmds[i]);
    }
    esp_console_start_repl(repl);
#else
    ESP_LOGW(TAG, "console USB-Serial-JTAG non configurée");
#endif
}
