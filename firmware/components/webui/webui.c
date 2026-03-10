/**
 * @file webui.c
 * @brief Embedded HTTP server Implementation.
 *
 * Serves a simple dashboard and provides REST API for:
 *   GET /             – Dashboard SPA
 *   GET /api/status   – uptime_ms, free_heap, version
 *   GET /api/tasks    – task cycle-time stats array
 *   GET /api/gpio     – current LED/GPIO state
 *   POST /api/gpio    – {"pin":<n>,"value":<0|1>}
 *   GET /api/config   – SSID, TCP port (read-only)
 */
#include "webui.h"
#include "utils.h"
#include "monitor.h"
#include "hw_hal.h"
#include "protocol.h"

#include <string.h>
#include <stdlib.h>
#include <esp_http_server.h>
#include <cJSON.h>

#ifdef CONFIG_PROJECT_WEBUI_PORT
#  define WEBUI_PORT CONFIG_PROJECT_WEBUI_PORT
#else
#  define WEBUI_PORT 80
#endif

#ifdef CONFIG_PROJECT_WIFI_SSID
#  define PROJ_SSID CONFIG_PROJECT_WIFI_SSID
#else
#  define PROJ_SSID "unknown"
#endif

#ifdef CONFIG_PROJECT_TCP_PORT
#  define PROJ_TCP_PORT CONFIG_PROJECT_TCP_PORT
#else
#  define PROJ_TCP_PORT 7070
#endif

#ifdef CONFIG_PROJECT_LED_GPIO
#  define LED_GPIO CONFIG_PROJECT_LED_GPIO
#else
#  define LED_GPIO 2
#endif

static const char *TAG = "WEBUI";
static httpd_handle_t s_server  = NULL;
static QueueHandle_t  s_cmd_queue = NULL;

/* ---- helpers ---- */
static void send_json(httpd_req_t *req, cJSON *root) {
    char *out = cJSON_PrintUnformatted(root);
    if (!out) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON Print failed");
    } else {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_sendstr(req, out);
        free(out);
    }
    cJSON_Delete(root);
}

/* --- Pin Capabilities --- */
#define CAP_GPIO_IN  (1 << 1)
#define CAP_GPIO_OUT (1 << 2)
#define CAP_PWM      (1 << 3)
#define CAP_ADC      (1 << 4)
#define CAP_DAC      (1 << 5)
#define CAP_I2C      (1 << 6)
#define CAP_UART     (1 << 7)

static uint32_t get_pin_caps(uint8_t pin) {
    uint32_t caps = CAP_GPIO_IN;
    /* Basic WROOM-32 Pinout logic */
    if (pin < 34) caps |= CAP_GPIO_OUT | CAP_PWM;
    if (pin == 32 || pin == 33 || pin == 34 || pin == 35 || pin == 36 || pin == 39 || 
        pin == 0 || pin == 2 || pin == 4 || pin == 12 || pin == 13 || pin == 14 || pin == 15 || pin == 25 || pin == 26 || pin == 27) {
        caps |= CAP_ADC;
    }
    if (pin == 25 || pin == 26) caps |= CAP_DAC;
    if (pin == 21 || pin == 22) caps |= CAP_I2C;
    if (pin == 16 || pin == 17) caps |= CAP_UART;
    return caps;
}

/* ---- GET / ---- */
static esp_err_t index_get_handler(httpd_req_t *req) {
    static const char html[] =
        "<!DOCTYPE html><html lang='en'><head>"
        "<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
        "<title>ESP32 Advanced Control</title>"
        "<link rel='stylesheet' href='https://fonts.googleapis.com/css2?family=Inter:wght@300;400;600&display=swap'>"
        "<style>"
        ":root{--bg:#0f172a;--card:rgba(30,41,59,0.7);--primary:#38bdf8;--accent:#818cf8;--text:#f1f5f9;--border:rgba(255,255,255,0.1);--green:#10b981;--red:#ef4444;}"
        "*{box-sizing:border-box;margin:0;padding:0;}"
        "body{font-family:'Inter',sans-serif;background:linear-gradient(135deg,#0f172a 0%,#1e293b 100%);color:var(--text);min-height:100vh;padding:20px;display:flex;justify-content:center;}"
        ".container{width:100%;max-width:1100px;}"
        "header{display:flex;justify-content:space-between;align-items:center;margin-bottom:30px;}"
        ".glass{background:var(--card);backdrop-filter:blur(12px);border:1px solid var(--border);border-radius:16px;padding:24px;box-shadow:0 8px 32px rgba(0,0,0,0.3);}"
        "h1{font-size:1.8rem;font-weight:600;background:linear-gradient(to right,var(--primary),var(--accent));-webkit-background-clip:text;-webkit-text-fill-color:transparent;}"
        ".tabs{display:flex;gap:10px;margin-bottom:20px;overflow-x:auto;padding-bottom:10px;}"
        ".tab{padding:10px 20px;border-radius:8px;cursor:pointer;transition:0.3s;background:rgba(255,255,255,0.05);border:1px solid var(--border);white-space:nowrap;}"
        ".tab.active{background:var(--primary);color:#000;font-weight:600;}"
        ".panel{display:none;animation:fadeIn 0.3s ease-out;}"
        ".panel.active{display:block;}"
        "@keyframes fadeIn{from{opacity:0;transform:translateY(5px);}to{opacity:1;transform:translateY(0);}}"
        ".grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(280px,1fr));gap:20px;}"
        ".pin-card{padding:20px;transition:0.3s;position:relative;}"
        ".pin-card:hover{border-color:var(--primary);transform:translateY(-2px);}"
        ".badge{font-size:0.7rem;padding:2px 6px;border-radius:4px;background:var(--primary);color:#000;font-weight:700;margin-left:8px;vertical-align:middle;}"
        ".row{display:flex;justify-content:space-between;align-items:center;margin-bottom:10px;}"
        "select,input{background:#000;border:1px solid var(--border);color:var(--text);padding:6px;border-radius:4px;font-size:0.9rem;}"
        "button{background:var(--primary);border:none;color:#000;padding:8px 16px;border-radius:6px;cursor:pointer;font-weight:600;transition:0.2s;}"
        "button:hover{filter:brightness(1.1);}"
        ".ctrl-group{margin-top:15px;padding-top:15px;border-top:1px solid var(--border);display:none;}"
        ".val-display{font-size:1.4rem;font-weight:600;color:var(--primary);}"
        "table{width:100%;border-collapse:collapse;margin-top:10px;}"
        "th,td{text-align:left;padding:12px;border-bottom:1px solid var(--border);}"
        "th{color:var(--primary);font-weight:600;font-size:0.8rem;text-transform:uppercase;}"
        "</style></head><body>"
        "<div class='container'><header><h1>ESP32 Advanced Control</h1><div id='uptime' style='font-size:0.9rem;opacity:0.7'></div></header>"
        "<div class='tabs'>"
        "  <div class='tab active' onclick='showTab(\"gpio\",this)'>IO Pin Mapping</div>"
        "  <div class='tab' onclick='showTab(\"periph\",this)'>Peripheral Status</div>"
        "  <div class='tab' onclick='showTab(\"tasks\",this)'>Task Timing</div>"
        "  <div class='tab' onclick='showTab(\"settings\",this)'>System</div>"
        "</div>"
        "<div id='gpio' class='panel active'><div class='glass'><h2>Universal Pin Configuration</h2>"
        "  <p style='margin-bottom:20px;opacity:0.6;font-size:0.9rem;'>Dynamically assign roles to any GPIO. Changes are persistent across reboots.</p>"
        "  <div class='grid' id='pins-grid'></div></div></div>"
        "<div id='periph' class='panel'><div class='glass'><h2>Hardware Peripherals</h2>"
"  <div style='display:grid;grid-template-columns:1fr 1fr;gap:20px;'>"
"    <div><h3>Sensor Data</h3><div id='adc-summary'></div></div>"
"    <div><h3>Protocol Configuration</h3>"
"      <div style='background:rgba(255,255,255,0.03);padding:15px;border-radius:8px;margin-bottom:15px;'>"
"        <b>I2C Master</b><br><small>SDA/SCL</small><br>"
"        <input id='i2c-sda' value='21' style='width:45px'> <input id='i2c-scl' value='22' style='width:45px'> "
"        <button onclick='cfgI2c()'>Apply</button></div>"
"      <div style='background:rgba(255,255,255,0.03);padding:15px;border-radius:8px;'>"
"        <b>UART2 (Secondary)</b><br><small>TX/RX</small><br>"
"        <input id='u2-tx' value='17' style='width:45px'> <input id='u2-rx' value='16' style='width:45px'> "
"        <button onclick='cfgUart()'>Apply</button></div>"
"    </div>"
"  </div></div></div>"
        "<div id='tasks' class='panel'><div class='glass'><h2>Realtime Task Monitoring</h2>"
        "  <table><thead><tr><th>Endpoint</th><th>Min (µs)</th><th>Avg (µs)</th><th>Max (µs)</th></tr></thead><tbody id='tasks-body'></tbody></table>"
        "</div></div>"
        "<div id='settings' class='panel'><div class='glass'><h2>System & Network</h2>"
        "  <div id='conf-info' style='margin-bottom:20px;'></div>"
        "  <h3>Firmware Controls</h3>"
        "  <div style='margin-top:10px;'>App Cycle: <input id='cycle-ms' value='100' style='width:60px'> ms <button onclick='setCycle()'>Update</button></div>"
        "  <div style='margin-top:20px;'><button style='background:var(--red);color:#fff' onclick='resetEsp()'>Hard Reset ESP32</button></div>"
        "</div></div>"
        "<script>"
        "const ROLES=['UNUSED','GPIO_IN','GPIO_OUT','PWM','ADC','DAC','I2C','UART'];"
        "function showTab(t,el){document.querySelectorAll('.panel').forEach(e=>e.classList.remove('active'));document.querySelectorAll('.tabs .tab').forEach(e=>e.classList.remove('active'));"
        "document.getElementById(t).classList.add('active');if(el)el.classList.add('active');}"
        "function resetEsp(){if(confirm('Restart Device?')) fetch('/api/config/sys',{method:'POST',body:JSON.stringify({reset:true})});}"
        "function setPinRole(p,r){fetch('/api/pin/cfg',{method:'POST',body:JSON.stringify({pin:p,role:parseInt(r)})}).then(()=>refreshPins());}"
        "function setPinVal(p,v){fetch('/api/gpio',{method:'POST',body:JSON.stringify({pin:p,value:parseInt(v)})});}"
        "function setPwm(p,d){fetch('/api/pwm',{method:'POST',body:JSON.stringify({pin:p,duty:parseInt(d)})});}"
        "function setDac(p,v){fetch('/api/dac',{method:'POST',body:JSON.stringify({pin:p,value:parseInt(v)})});}"
"function cfgI2c(){fetch('/api/config/hw',{method:'POST',body:JSON.stringify({type:'i2c',sda:parseInt(document.getElementById('i2c-sda').value),scl:parseInt(document.getElementById('i2c-scl').value)})});}"
"function cfgUart(){fetch('/api/config/hw',{method:'POST',body:JSON.stringify({type:'uart',tx:parseInt(document.getElementById('u2-tx').value),rx:parseInt(document.getElementById('u2-rx').value)})});}"
        "function refreshPins(){fetch('/api/gpio/schema').then(r=>r.json()).then(data=>{"
        "  const grid=document.getElementById('pins-grid');grid.innerHTML=''; data.forEach(p=>{"
        "    const card=document.createElement('div');card.className='pin-card glass';"
        "    let html=`<div class='row'><b>Pin ${p.pin}</b><span class='badge'>${p.role_name}</span></div>`;"
        "    html+=`<select onchange='setPinRole(${p.pin},this.value)'>`;"
        "    ROLES.forEach((rn,idx)=>{ if((p.caps&(1<<idx))||idx==0) html+=`<option value='${idx}' ${idx==p.role?'selected':''}>${rn}</option>`; });"
        "    html+='</select>';"
        "    if(p.role==2){ html+=`<div class='ctrl-group' style='display:block'><button onclick='setPinVal(${p.pin},${p.val?0:1})'>${p.val?'Turn OFF':'Turn ON'}</button></div>`; }"
        "    else if(p.role==3){ html+=`<div class='ctrl-group' style='display:block'><small>Duty Cycle</small><br><input type='range' min='0' max='1023' value='${p.val}' onchange='setPwm(${p.pin},this.value)'></div>`; }"
        "    else if(p.role==4){ html+=`<div class='ctrl-group' style='display:block'><div class='val-display'>${p.mv}</div><small>Millivolts</small></div>`; }"
        "    else if(p.role==5){ html+=`<div class='ctrl-group' style='display:block'><small>Voltage (0-255)</small><br><input type='range' min='0' max='255' value='${p.val}' onchange='setDac(${p.pin},this.value)'></div>`; }"
        "    card.innerHTML=html; grid.appendChild(card);"
        "  }); });}"
        "setInterval(()=>{ refreshPins(); "
        "  if(document.getElementById('tasks').classList.contains('active')){"
        "    fetch('/api/tasks').then(r=>r.json()).then(a=>{document.getElementById('tasks-body').innerHTML=a.map(t=>`<tr><td>${t.name}</td><td>${t.min_us}</td><td>${t.avg_us}</td><td>${t.max_us}</td></tr>`).join('');});"
        "  }"
        "},5000); refreshPins();" // initial load
        "fetch('/api/config').then(r=>r.json()).then(c=>{document.getElementById('conf-info').innerHTML=`WiFi Station: <b>${c.ssid}</b><br>EPICS ASCII Port: <b>${c.tcp_port}</b>`;});"
        "</script></body></html>";
    httpd_resp_sendstr(req, html);
    return ESP_OK;
}

/* ---- GET /api/status ---- */
static esp_err_t status_get_handler(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "uptime_ms",  (double)monitor_get_uptime_ms());
    cJSON_AddNumberToObject(root, "free_heap",  (double)monitor_get_free_heap());
    cJSON_AddStringToObject(root, "version",    "1.1.0");
    send_json(req, root);
    return ESP_OK;
}

/* ---- GET /api/tasks ---- */
static esp_err_t tasks_get_handler(httpd_req_t *req) {
    size_t count = monitor_get_task_count();
    cJSON *arr = cJSON_CreateArray();
    
    for (size_t i = 0; i < count; i++) {
        char name[MONITOR_NAME_LEN];
        if (monitor_get_task_name(i, name)) {
            uint32_t mn, mx, av;
            if (monitor_get_stats(name, &mn, &mx, &av)) {
                cJSON *t = cJSON_CreateObject();
                cJSON_AddStringToObject(t, "name",   name);
                cJSON_AddNumberToObject(t, "min_us", (double)mn);
                cJSON_AddNumberToObject(t, "max_us", (double)mx);
                cJSON_AddNumberToObject(t, "avg_us", (double)av);
                cJSON_AddItemToArray(arr, t);
            }
        }
    }
    send_json(req, arr);
    return ESP_OK;
}


/* ---- POST /api/gpio ---- */
static esp_err_t gpio_post_handler(httpd_req_t *req) {
    char buf[128] = {0};
    int  len = req->content_len < (int)sizeof(buf) - 1 ? req->content_len : (int)sizeof(buf) - 1;
    if (httpd_req_recv(req, buf, len) <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
        return ESP_FAIL;
    }
    cJSON *body = cJSON_Parse(buf);
    if (!body) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad JSON");
        return ESP_FAIL;
    }
    cJSON *jpin = cJSON_GetObjectItem(body, "pin");
    cJSON *jval = cJSON_GetObjectItem(body, "value");
    if (jpin && jval) {
        int pin = jpin->valueint;
        int val = jval->valueint;
        hw_hal_gpio_set(pin, val ? true : false);
    }
    cJSON_Delete(body);

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", "ok");
    send_json(req, resp);
    return ESP_OK;
}

/* ---- GET /api/gpio/schema ---- */
static esp_err_t gpio_schema_get_handler(httpd_req_t *req) {
    cJSON *arr = cJSON_CreateArray();
    const char *role_names[] = {"UNUSED","GPIO_IN","GPIO_OUT","PWM","ADC","DAC","I2C","UART"};
    
    for (int i = 0; i < 40; i++) {
        /* Skip SPI flash pins (6-11) */
        if (i >= 6 && i <= 11) continue;
        
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "pin", i);
        uint32_t caps = get_pin_caps(i);
        cJSON_AddNumberToObject(o, "caps", caps);
        
        pin_role_t role = hw_hal_get_pin_role(i);
        cJSON_AddNumberToObject(o, "role", (int)role); 
        cJSON_AddStringToObject(o, "role_name", role_names[role]);
        
        if (role == PIN_ROLE_PWM || role == PIN_ROLE_DAC) {
            cJSON_AddNumberToObject(o, "val", hw_hal_get_pin_value(i));
        } else {
            cJSON_AddNumberToObject(o, "val", hw_hal_gpio_get(i));
        }
        
        if (caps & CAP_ADC) cJSON_AddNumberToObject(o, "mv", hw_hal_adc_read_mv(i));
        
        cJSON_AddItemToArray(arr, o);
    }
    send_json(req, arr);
    return ESP_OK;
}

/* ---- GET /api/config ---- */
static esp_err_t config_get_handler(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "ssid",     PROJ_SSID);
    cJSON_AddNumberToObject(root, "tcp_port", PROJ_TCP_PORT);
    cJSON_AddNumberToObject(root, "led_pin",  LED_GPIO);
    send_json(req, root);
    return ESP_OK;
}

/* ---- POST /api/pin/cfg ---- */
static esp_err_t pin_cfg_post_handler(httpd_req_t *req) {
    char buf[128] = {0};
    if (httpd_req_recv(req, buf, req->content_len) <= 0) return ESP_FAIL;
    cJSON *body = cJSON_Parse(buf);
    if (!body) return ESP_FAIL;
    int pin = cJSON_GetObjectItem(body, "pin")->valueint;
    int role = cJSON_GetObjectItem(body, "role")->valueint;
    hw_hal_pin_cfg((uint8_t)pin, (pin_role_t)role);
    cJSON_Delete(body);
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

/* ---- POST /api/dac ---- */
static esp_err_t dac_post_handler(httpd_req_t *req) {
    char buf[128] = {0};
    if (httpd_req_recv(req, buf, req->content_len) <= 0) return ESP_FAIL;
    cJSON *body = cJSON_Parse(buf);
    if (!body) return ESP_FAIL;
    int pin = cJSON_GetObjectItem(body, "pin")->valueint;
    int val = cJSON_GetObjectItem(body, "value")->valueint;
    hw_hal_dac_set_voltage((uint8_t)pin, (uint8_t)val);
    cJSON_Delete(body);
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}


/* ---- POST /api/pwm ---- */
static esp_err_t pwm_post_handler(httpd_req_t *req) {
    char buf[128] = {0};
    int  len = req->content_len < (int)sizeof(buf) - 1 ? req->content_len : (int)sizeof(buf) - 1;
    if (httpd_req_recv(req, buf, len) <= 0) return ESP_FAIL;
    
    cJSON *body = cJSON_Parse(buf);
    if (!body) return ESP_FAIL;
    
    cJSON *jpin = cJSON_GetObjectItem(body, "pin");
    cJSON *jduty = cJSON_GetObjectItem(body, "duty");
    if (jpin && jduty) {
        hw_hal_pwm_set_duty((uint8_t)jpin->valueint, (uint32_t)jduty->valueint);
    }
    cJSON_Delete(body);
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

/* ---- POST /api/config/hw ---- */
static esp_err_t hw_config_post_handler(httpd_req_t *req) {
    char buf[256] = {0};
    int  len = req->content_len < (int)sizeof(buf) - 1 ? req->content_len : (int)sizeof(buf) - 1;
    if (httpd_req_recv(req, buf, len) <= 0) return ESP_FAIL;
    
    cJSON *body = cJSON_Parse(buf);
    if (!body) return ESP_FAIL;
    
    cJSON *jtype = cJSON_GetObjectItem(body, "type");
    if (jtype && strcmp(jtype->valuestring, "i2c") == 0) {
        int sda = cJSON_GetObjectItem(body, "sda")->valueint;
        int scl = cJSON_GetObjectItem(body, "scl")->valueint;
        hw_hal_i2c_init(sda, scl, 100000);
        UTILS_LOGI(TAG, "WebUI Reconf I2C: SDA=%d SCL=%d", sda, scl);
    } else if (jtype && strcmp(jtype->valuestring, "uart") == 0) {
        int tx = cJSON_GetObjectItem(body, "tx")->valueint;
        int rx = cJSON_GetObjectItem(body, "rx")->valueint;
        hw_hal_uart2_init(tx, rx, 9600);
        UTILS_LOGI(TAG, "WebUI Reconf UART2: TX=%d RX=%d", tx, rx);
    }
    
    cJSON_Delete(body);
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

/* ---- POST /api/config/sys ---- */
static esp_err_t sys_config_post_handler(httpd_req_t *req) {
    char buf[128] = {0};
    int  len = req->content_len < (int)sizeof(buf) - 1 ? req->content_len : (int)sizeof(buf) - 1;
    if (httpd_req_recv(req, buf, len) <= 0) return ESP_FAIL;
    cJSON *body = cJSON_Parse(buf);
    if (!body) return ESP_FAIL;
    cJSON *jcycle = cJSON_GetObjectItem(body, "cycle_ms");
    if (jcycle) {
        protocol_cmd_t cmd = {
            .device = DEVICE_SYS,
            .action = ACTION_CYCLE,
            .value  = jcycle->valueint,
            .response_fd = -1
        };
        xQueueSend(s_cmd_queue, &cmd, 0);
    }
    cJSON_Delete(body);
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}


/* ---- URI table ---- */
static const httpd_uri_t uris[] = {
    { .uri = "/",            .method = HTTP_GET,  .handler = index_get_handler,  .user_ctx = NULL },
    { .uri = "/api/status",  .method = HTTP_GET,  .handler = status_get_handler, .user_ctx = NULL },
    { .uri = "/api/tasks",   .method = HTTP_GET,  .handler = tasks_get_handler,  .user_ctx = NULL },
    { .uri = "/api/gpio/schema", .method = HTTP_GET, .handler = gpio_schema_get_handler, .user_ctx = NULL },
    { .uri = "/api/pin/cfg", .method = HTTP_POST, .handler = pin_cfg_post_handler, .user_ctx = NULL },
    { .uri = "/api/dac",     .method = HTTP_POST, .handler = dac_post_handler,     .user_ctx = NULL },
    { .uri = "/api/pwm",     .method = HTTP_POST, .handler = pwm_post_handler,   .user_ctx = NULL },
    { .uri = "/api/config/hw", .method = HTTP_POST, .handler = hw_config_post_handler, .user_ctx = NULL },
    { .uri = "/api/config/sys",.method = HTTP_POST, .handler = sys_config_post_handler,.user_ctx = NULL },
    { .uri = "/api/gpio",    .method = HTTP_POST, .handler = gpio_post_handler,  .user_ctx = NULL },
    { .uri = "/api/config",  .method = HTTP_GET,  .handler = config_get_handler, .user_ctx = NULL },
};


void webui_init(QueueHandle_t cmd_queue) {
    s_cmd_queue = cmd_queue;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port   = WEBUI_PORT;
    config.max_uri_handlers = 16; // Increased to accommodate all API endpoints

    UTILS_LOGI(TAG, "Starting server on port %d", config.server_port);
    if (httpd_start(&s_server, &config) == ESP_OK) {
        for (size_t i = 0; i < sizeof(uris)/sizeof(uris[0]); i++) {
            httpd_register_uri_handler(s_server, &uris[i]);
        }
        UTILS_LOGI(TAG, "Registered %d URI handlers", (int)(sizeof(uris)/sizeof(uris[0])));
    } else {
        UTILS_LOGE(TAG, "Failed to start HTTP server");
    }
}

void webui_stop(void) {
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
    }
}
