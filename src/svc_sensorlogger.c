#include "ets_sys.h"
#include "os_type.h"
#include "mem.h"
#include "osapi.h"
#include "user_interface.h"

#include "espconn.h"
#include "gpio.h"
#include "driver/uart.h"
#include "microrl.h"
#include "console.h"
#include "httpclient.h"
#include <sensorlogger.h>
#include <stdlib.h>
#include <generic/macros.h>
#include <main.h>


static double get_something(struct slogger_data_type *tp)
{
	return system_get_time() / 1000000.0;
}

ETSTimer schedTimer;

static struct slogger_instance this = {
	.deviceType			= "dummy",
	.deviceGroup		= "development",
	.deviceName			= "frankenstein-sensorlogger",
};


struct slogger_instance  *svclog_get_global_instance()
{
		return &this;
}

#define load_param(key) \
	tmp = env_get("slog-" #key); \
	if (tmp) { \
		this.key = tmp; \
	}

static void svclog_load_env()
{
	char *tmp;
	load_param(deviceId);
	load_param(deviceType);
	load_param(deviceGroup);
	load_param(deviceParentGroup);
	load_param(deviceName);
	load_param(userName);
	load_param(password);
	load_param(nextCloudUrl);

	if (!this.deviceId) {
		asprintf(&this.deviceId, "frankenstein-%x", system_get_chip_id());
	}

	if (!this.userName)
		console_printf("[!] Please set slog-userName or senslog will not work correctly\n");
	if (!this.nextCloudUrl)
		console_printf("[!] Please set slog-nextCloudUrl or senslog will not work correctly\n");
	if (!this.password)
		console_printf("[!] Please set slog-password or senslog will not work correctly\n");
}

static struct slogger_http_request *cur_rq;


static void request_finalize() {
	console_lock(0);
	char *tmp = cur_rq->userdata;
	slogger_http_request_release(cur_rq);
	cur_rq = NULL;
	if (tmp)
		console_write(tmp, strlen(tmp));
}

void http_cb(char *response_body, int http_status, char *response_headers, int body_size)
{
	console_printf("http status: %d\n", http_status);
	if (http_status != HTTP_STATUS_GENERIC_ERROR) {
		console_printf("server response: %s\n", response_body);
	}
	request_finalize();
}

void http_data_type_cb(char *response_body, int http_status, char *response_headers, int body_size)
{
	console_printf("http status: %d\n", http_status);
	if (http_status != HTTP_STATUS_GENERIC_ERROR) {
		console_printf("server resp: %s\n", response_body);
		slogger_instance_populate_data_type_ids(&this, response_body);
	}
	request_finalize();
}


void add_dummy()
{
	console_printf("senslogger: Registering dummy sensors\n");
	struct slogger_data_type *dt = malloc(sizeof(*dt));

	dt->type		= "mana";
	dt->description	= "Mana";
	dt->unit		= "pts";
	dt->get_current_value = get_something;
	sensorlogger_instance_register_data_type(&this, dt);

	dt = malloc(sizeof(*dt));

	dt->type		= "health";
	dt->description	= "Health";
	dt->unit		= "pts";
	dt->get_current_value = get_something;
	sensorlogger_instance_register_data_type(&this, dt);
}

#ifdef CONFIG_SENSLOG_DUMMY
FR_CONSTRUCTOR(dummy_reg)
{
	add_dummy();
}
#endif


static void rearm();
static void autopost() {
	struct slogger_http_request *rq = NULL;
	http_callback cb = http_cb;
	svclog_load_env();

	if (cur_rq)
		return;

	if (!slogger_instance_is_ready(&this)) {
		rq = slogger_instance_rq_get_data_types(&this);
		cb = http_data_type_cb;
	} else {
		rq = slogger_instance_rq_post(&this);
	}

	cur_rq = rq;
	http_post(rq->url, rq->data, rq->headers, cb);
	rearm();
}

static void rearm()
{
	char *period = env_get("slog-interval");
	if (period) {
		uint32_t tm = atoi(period);
		os_timer_disarm(&schedTimer);
		os_timer_setfn(&schedTimer, (os_timer_func_t *)autopost, NULL);
		os_timer_arm(&schedTimer, tm, 0);
	}
}

FR_CONSTRUCTOR(periodic_post)
{
	char *period = env_get("slog-interval");
	if (period) {
		uint32_t tm = atoi(period);
		console_printf("senslog: Will post data every %d ms\n", tm);
	}
	rearm();
}

static int   do_svclog(int argc, const char *const *argv)
{
	struct slogger_http_request *rq = NULL;
	http_callback cb = http_cb;
	svclog_load_env();


	if (strcmp(argv[1], "create") == 0) {
		/* type description unit */
		if (argc < 5) {
			console_printf("Need at least 4 arguments, have %d", argc);
			return 0;
		}
		struct slogger_data_type *dt = malloc(sizeof(*dt));
		dt->type		= strdup(argv[2]);
		dt->description	= strdup(argv[3]);
		dt->unit		= strdup(argv[4]);
		dt->get_current_value = NULL;
		dt->current_value = 0;
		sensorlogger_instance_register_data_type(&this, dt);
	} else if (strcmp(argv[1], "add_dummy") == 0) {
		add_dummy();
	} else if (strcmp(argv[1], "set") == 0) {
		if (argc < 5) {
			console_printf("Need at least 4 arguments, have %d", argc);
			return 0;
		}
		double value = strtod(argv[5], NULL);
		slogger_instance_set_current_value(&this,
			argv[3], argv[2], argv[4], value);
	} else if (strcmp(argv[1], "register") == 0) {
		rq = slogger_instance_rq_register(&this);
		rq->userdata = "senslog get_dt\n\r\n\r";
		console_printf("Registering device at %s\n", rq->url);
	} else if (strcmp(argv[1], "post") == 0) {
		rq = slogger_instance_rq_post(&this);
		console_printf("Posting data to %s\n", rq->url);
    } else if (strcmp(argv[1], "dump") == 0) {
		slogger_instance_dump(&this);
	} else if (strcmp(argv[1], "get_dt") == 0) {
		rq = slogger_instance_rq_get_data_types(&this);
		cb = http_data_type_cb;
		console_printf("Requesting info at %s\n", rq->url);
	} else {
		console_printf("Unknown op %s\n", argv[1]);
	}

	if (!rq)
		return 0;
	cur_rq = rq;
	console_printf("%s\n", rq->data);
	http_post(rq->url, rq->data, rq->headers, cb);
	console_lock(1);
	return 0;
}

static void do_svclog_interrupt(void)
{
}


CONSOLE_CMD(senslog, 2, -1,
	    do_svclog, do_svclog_interrupt, NULL,
	    "Register and send data to a nextcloud sensorlogger"
	    HELPSTR_NEWLINE "senslog dump       						     - Print all info about sensorlogger and registered data types"
	    HELPSTR_NEWLINE "senslog register       						 - Registers this device with nextcloud and obtain data type ids (Implies get_dt)"
	    HELPSTR_NEWLINE "senslog get_dt         						 - obtains data type ids from server"
	    HELPSTR_NEWLINE "senslog post           						 - posts data from all configured sensors"
	    HELPSTR_NEWLINE "senslog create type description unit            - Create a new datatype from commandline"
	    HELPSTR_NEWLINE "senslog set type description unit current_value - Update current sensor value from commandline"
	    );
