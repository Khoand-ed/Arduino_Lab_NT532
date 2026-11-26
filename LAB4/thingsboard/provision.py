"""
LAB 4 - KHỞI TẠO THINGSBOARD CHO NHÓM (chạy trên máy tính, sau khi Docker đã chạy)

Cấu hình tự động ThingsBoard CE chạy local (Docker) qua REST API, theo config.json:
  1. Đăng nhập sysadmin, tạo tenant "NT532.R12 - Nhom 10" và tài khoản tenant admin,
     kích hoạt luôn bằng mật khẩu trong config.json (không cần gửi mail).
  2. Đăng nhập tenant admin, tạo 2 device profile: ESP32-BH1750-Node, RPi-DHT11-Node.
  3. Tạo 2 device với access token cố định (ESP32-BH1750, RaspberryPi-DHT11) và đặt
     server attribute inactivityTimeout = 30000 ms: thiết bị im lặng ~30 s thì
     "active" chuyển sang false. Lưu ý:
       - Gói PINGREQ (keepalive MQTT) cũng tính là hoạt động: thiết bị còn giữ kết nối
         MQTT thì vẫn "active" dù không gửi telemetry, VỚI ĐIỀU KIỆN keepalive nhỏ hơn hẳn
         30 s (ESP32 15 s, Raspberry Pi 10 s; để 30 s thì cứ ~1 phút lại báo Offline/Online).
         Mất điện / mất Wi-Fi / ngắt kết nối thì mới chuyển sang không hoạt động.
       - ThingsBoard kiểm tra trạng thái theo chu kỳ DEFAULT_STATE_CHECK_INTERVAL. File
         LAB4/server/docker-compose.yml đặt 10 s (mặc định của ThingsBoard là 60 s), nên
         "active" chuyển sang false khoảng 30..40 s sau khi thiết bị ngắt kết nối.
  4. Bổ sung Root Rule Chain (giữ nguyên các luồng mặc định): mỗi sự kiện
     CONNECTED / DISCONNECTED / ACTIVE / INACTIVE, mỗi lệnh RPC server -> thiết bị
     (vd "RPC setLed1(true)") và kết quả của lệnh ("RPC DELIVERED" / "RPC OK: true",
     "RPC FAILED: NO_ACTIVE_CONNECTION" khi thiết bị offline) được
     lưu thành telemetry "sysEvent" của thiết bị. Thiết bị tự gửi sự kiện của mình (BOOT,
     MQTT RECONNECT, DHT11 read error...) vào key "event". Hai key tách riêng và mỗi loại
     sự kiện có ts riêng nên các dòng nhật ký không ghi đè nhau khi đến cùng mili giây.
     "sysEvent" + "event" là nguồn dữ liệu cho bảng nhật ký hệ thống trên dashboard.
  5. Nạp widgets bundle "Nhom 10 - Lab 4" (widgets/*.json): widget tự viết cho tiêu đề/điều
     hướng, tổng quan kết nối, thẻ giá trị cảm biến, bảng Logs (lọc + tải CSV/Excel).
     Widget type nhận biết theo fqn, bundle theo alias/tiêu đề: có rồi thì cập nhật tại chỗ.
  6. Nạp dashboard "Lab 4 - IoT Dashboard (Nhom 10)" (dashboard_lab4.json), cập nhật tại chỗ
     theo tiêu đề. Entity alias lọc theo TÊN thiết bị / DEVICE PROFILE nên không chứa UUID cố
     định (kiểm tra trước mỗi alias tìm thấy bao nhiêu thiết bị). Đặt làm home dashboard của
     tenant: đăng nhập tenant admin sẽ thấy dashboard ngay ở trang Home.
     Lưu ý: file JSON là bản gốc. Sửa dashboard / widget trên web thì Export đè lại vào file
     (dashboard_lab4.json, widgets/...), nếu không lần chạy sau sẽ ghi đè bản trên web.
  7. In tóm tắt: URL, tài khoản đăng nhập, id và access token của thiết bị, link dashboard.

Chạy lại nhiều lần vẫn an toàn (idempotent): phần nào đã có thì bỏ qua, chỉ sửa
phần khác với config.json / file JSON. Lần chạy thứ hai sẽ báo "không có thay đổi".

Cài thư viện:  pip install -r requirements.txt
Chạy:          python provision.py
               python provision.py --url http://localhost:8080 --config config.json
               python provision.py --no-home-dashboard     # không đặt home dashboard
"""

import argparse
import copy
import glob
import json
import os
import sys
from urllib.parse import parse_qs, urlparse

import requests

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DEFAULT_CONFIG = os.path.join(BASE_DIR, "config.json")
WIDGETS_DIR = os.path.join(BASE_DIR, "widgets")              # widgets bundle (định dạng export của ThingsBoard)
DASHBOARD_FILE = os.path.join(BASE_DIR, "dashboard_lab4.json")

# Tiền tố tên các rule node do script này thêm vào Root Rule Chain (dùng để nhận biết
# khi chạy lại, không thêm trùng)
RULE_NODE_PREFIX = "Lab4 "


# ----------------------------------------------------------------------------
# Gọi REST API của ThingsBoard
# ----------------------------------------------------------------------------
class TbApiError(Exception):
    """Lỗi trả về từ REST API của ThingsBoard."""

    def __init__(self, method, path, status, message):
        super().__init__(f"{method} {path} -> HTTP {status}: {message}")
        self.status = status
        self.message = message


class TbClient:
    """Phiên làm việc REST với ThingsBoard bằng một tài khoản (JWT)."""

    def __init__(self, base_url, username, password, timeout=15):
        self.base_url = base_url.rstrip("/")
        self.username = username
        self.password = password
        self.timeout = timeout
        self.session = requests.Session()
        self.session.headers["Content-Type"] = "application/json"

    def login(self):
        """Đăng nhập, lưu JWT vào header X-Authorization."""
        self.session.headers.pop("X-Authorization", None)
        data = self.request("POST", "/api/auth/login",
                            json={"username": self.username, "password": self.password})
        self.session.headers["X-Authorization"] = "Bearer " + data["token"]
        return self

    def request(self, method, path, *, params=None, json=None, allow_404=False):
        """Gọi API; trả về JSON (hoặc text), None nếu 404 và allow_404=True."""
        url = self.base_url + path
        try:
            response = self.session.request(method, url, params=params, json=json,
                                            timeout=self.timeout)
        except requests.RequestException as error:
            raise SystemExit(f"Không kết nối được ThingsBoard tại {self.base_url} ({error}).\n"
                             "Kiểm tra container: docker compose ps (thư mục LAB4/server)")
        if response.status_code == 404 and allow_404:
            return None
        if response.status_code >= 400:
            try:
                message = response.json().get("message", response.text)
            except ValueError:
                message = response.text
            raise TbApiError(method, path, response.status_code, message)
        if not response.content:
            return None
        if "application/json" in response.headers.get("Content-Type", ""):
            return response.json()
        return response.text

    def get(self, path, **kwargs):
        return self.request("GET", path, **kwargs)

    def post(self, path, **kwargs):
        return self.request("POST", path, **kwargs)

    def delete(self, path, **kwargs):
        return self.request("DELETE", path, **kwargs)

    def find_in_pages(self, path, predicate, params=None, page_size=100):
        """Duyệt API dạng phân trang (pageSize/page), trả về phần tử đầu tiên thỏa predicate."""
        found = self.find_all_in_pages(path, predicate, params, page_size, first_only=True)
        return found[0] if found else None

    def find_all_in_pages(self, path, predicate, params=None, page_size=100, first_only=False):
        """Duyệt API dạng phân trang, trả về danh sách mọi phần tử thỏa predicate."""
        result, page = [], 0
        while True:
            query = dict(params or {}, pageSize=page_size, page=page)
            data = self.get(path, params=query)
            for item in data.get("data", []):
                if predicate(item):
                    result.append(item)
                    if first_only:
                        return result
            if not data.get("hasNext"):
                return result
            page += 1


def entity_id(entity_type, uuid):
    return {"entityType": entity_type, "id": uuid}


# ----------------------------------------------------------------------------
# Trạng thái dùng chung giữa các bước
# ----------------------------------------------------------------------------
class Context:
    """Dữ liệu chia sẻ giữa các bước provision (bước sau dùng kết quả bước trước)."""

    def __init__(self, config, base_url):
        self.config = config
        self.base_url = base_url.rstrip("/")
        self.sysadmin = None          # TbClient của sysadmin
        self.tenant_api = None        # TbClient của tenant admin
        self.tenant_id = None
        self.tenant_admin_id = None
        self.profile_ids = {}         # tên device profile -> id
        self.devices = {}             # tên device -> {"id", "token", "label", "profile"}
        self.root_rule_chain_id = None
        self.set_home_dashboard = True  # tắt bằng --no-home-dashboard
        self.changes = 0              # số thay đổi đã thực hiện trong lần chạy này
        self.summary_extra = []       # các bước sau (widget, dashboard) thêm dòng tóm tắt

    def changed(self, message):
        self.changes += 1
        print(f"  [+] {message}")

    @staticmethod
    def unchanged(message):
        print(f"  [=] {message}")


# ----------------------------------------------------------------------------
# Bước 1: tenant + tenant admin (dùng tài khoản sysadmin)
# ----------------------------------------------------------------------------
def step_tenant(ctx):
    cfg_sys = ctx.config["sysadmin"]
    cfg_tenant = ctx.config["tenant"]
    ctx.sysadmin = TbClient(ctx.base_url, cfg_sys["email"], cfg_sys["password"]).login()
    print(f"  Đăng nhập sysadmin {cfg_sys['email']} thành công")

    title = cfg_tenant["title"]
    tenant = ctx.sysadmin.find_in_pages("/api/tenants", lambda t: t.get("title") == title,
                                        params={"textSearch": title})
    if tenant is None:
        tenant = ctx.sysadmin.post("/api/tenant", json={
            "title": title,
            "description": cfg_tenant.get("description", ""),
        })
        ctx.changed(f"Tạo tenant \"{title}\"")
    else:
        ctx.unchanged(f"Tenant \"{title}\" đã có")
    ctx.tenant_id = tenant["id"]["id"]


def step_tenant_admin(ctx):
    cfg_admin = ctx.config["tenant_admin"]
    email = cfg_admin["email"]
    user = ctx.sysadmin.find_in_pages(f"/api/tenant/{ctx.tenant_id}/users",
                                      lambda u: u.get("email", "").lower() == email.lower(),
                                      params={"textSearch": email})
    if user is None:
        user = ctx.sysadmin.post("/api/user", params={"sendActivationMail": "false"}, json={
            "email": email,
            "authority": "TENANT_ADMIN",
            "tenantId": entity_id("TENANT", ctx.tenant_id),
            "firstName": cfg_admin.get("first_name", ""),
            "lastName": cfg_admin.get("last_name", ""),
        })
        ctx.changed(f"Tạo tenant admin {email}")
    else:
        ctx.unchanged(f"Tenant admin {email} đã có")
    ctx.tenant_admin_id = user["id"]["id"]

    # Đăng nhập thử: được thì tài khoản đã kích hoạt đúng mật khẩu
    ctx.tenant_api = TbClient(ctx.base_url, email, cfg_admin["password"])
    try:
        ctx.tenant_api.login()
        ctx.unchanged("Tenant admin đã kích hoạt, đăng nhập thành công")
        return
    except TbApiError as error:
        if error.status not in (400, 401, 403):   # tài khoản chưa kích hoạt / sai mật khẩu
            raise

    # Chưa kích hoạt: lấy activation link (chỉ sysadmin xem được) rồi đặt mật khẩu
    try:
        link = ctx.sysadmin.get(f"/api/user/{ctx.tenant_admin_id}/activationLink")
    except TbApiError as error:
        raise SystemExit(f"Tenant admin {email} đã kích hoạt nhưng mật khẩu khác config.json "
                         f"({error.message}). Đổi mật khẩu trên web hoặc sửa config.json.")
    token = parse_qs(urlparse(str(link).strip().strip('"')).query).get("activateToken", [None])[0]
    if not token:
        raise SystemExit(f"Không đọc được activateToken từ activation link: {link}")
    ctx.sysadmin.post("/api/noauth/activate", params={"sendActivationMail": "false"},
                      json={"activateToken": token, "password": cfg_admin["password"]})
    ctx.tenant_api.login()
    ctx.changed(f"Kích hoạt tenant admin {email} với mật khẩu trong config.json")


# ----------------------------------------------------------------------------
# Bước 2: device profile (dùng tài khoản tenant admin)
# ----------------------------------------------------------------------------
def step_device_profiles(ctx):
    api = ctx.tenant_api
    for cfg_profile in ctx.config["device_profiles"]:
        name = cfg_profile["name"]
        profile = api.find_in_pages("/api/deviceProfileInfos", lambda p: p.get("name") == name,
                                    params={"textSearch": name})
        if profile is None:
            profile = api.post("/api/deviceProfile", json={
                "name": name,
                "description": cfg_profile.get("description", ""),
                "type": "DEFAULT",
                "transportType": "DEFAULT",       # MQTT/HTTP/CoAP với topic mặc định
                "provisionType": "DISABLED",
                "profileData": {
                    "configuration": {"type": "DEFAULT"},
                    "transportConfiguration": {"type": "DEFAULT"},
                    "provisionConfiguration": {"type": "DISABLED", "provisionDeviceSecret": None},
                    "alarms": None,
                },
                # Không chỉ định rule chain riêng -> dùng Root Rule Chain của tenant
                "defaultRuleChainId": None,
            })
            ctx.changed(f"Tạo device profile {name}")
        else:
            ctx.unchanged(f"Device profile {name} đã có")
        ctx.profile_ids[name] = profile["id"]["id"]


# ----------------------------------------------------------------------------
# Bước 3: device + access token + inactivityTimeout
# ----------------------------------------------------------------------------
def ensure_device(ctx, cfg_device):
    api = ctx.tenant_api
    name, token = cfg_device["name"], cfg_device["access_token"]
    profile_id = ctx.profile_ids[cfg_device["profile"]]

    device = api.get("/api/tenant/devices", params={"deviceName": name}, allow_404=True)
    if device is None:
        device = api.post("/api/device", params={"accessToken": token}, json={
            "name": name,
            "label": cfg_device.get("label", ""),
            "deviceProfileId": entity_id("DEVICE_PROFILE", profile_id),
        })
        ctx.changed(f"Tạo device {name} (token {token})")
    else:
        ctx.unchanged(f"Device {name} đã có")
        if (device.get("label") != cfg_device.get("label", "")
                or device["deviceProfileId"]["id"] != profile_id):
            device["label"] = cfg_device.get("label", "")
            device["deviceProfileId"] = entity_id("DEVICE_PROFILE", profile_id)
            device = api.post("/api/device", json=device)
            ctx.changed(f"Cập nhật label/profile của {name}")

    device_id = device["id"]["id"]
    credentials = api.get(f"/api/device/{device_id}/credentials")
    if credentials.get("credentialsType") != "ACCESS_TOKEN" or credentials.get("credentialsId") != token:
        credentials.update({"credentialsType": "ACCESS_TOKEN", "credentialsId": token,
                            "credentialsValue": None})
        api.post("/api/device/credentials", json=credentials)
        ctx.changed(f"Đổi access token của {name} thành {token}")
    else:
        ctx.unchanged(f"Access token của {name} đúng ({token})")

    timeout = int(ctx.config["inactivity_timeout_ms"])
    attributes = api.get(f"/api/plugins/telemetry/DEVICE/{device_id}/values/attributes/SERVER_SCOPE",
                         params={"keys": "inactivityTimeout"}) or []
    current = next((a["value"] for a in attributes if a.get("key") == "inactivityTimeout"), None)
    if current is None or int(current) != timeout:
        api.post(f"/api/plugins/telemetry/DEVICE/{device_id}/attributes/SERVER_SCOPE",
                 json={"inactivityTimeout": timeout})
        ctx.changed(f"Đặt inactivityTimeout = {timeout} ms cho {name}")
    else:
        ctx.unchanged(f"inactivityTimeout của {name} = {timeout} ms")

    ctx.devices[name] = {"id": device_id, "token": token, "label": cfg_device.get("label", ""),
                         "profile": cfg_device["profile"], "key": cfg_device.get("key")}


def step_devices(ctx):
    for cfg_device in ctx.config["devices"]:
        ensure_device(ctx, cfg_device)


# ----------------------------------------------------------------------------
# Bước 4: Root Rule Chain -> telemetry "sysEvent" (nhật ký hệ thống)
# ----------------------------------------------------------------------------
# Key telemetry chứa sự kiện do ThingsBoard sinh ra (rule chain ghi). Tách riêng khỏi key
# "event" mà thiết bị tự gửi (BOOT, MQTT RECONNECT, DHT11 read error...): ThingsBoard lưu mỗi
# (thiết bị, key, ts) đúng MỘT giá trị, hai bản tin cùng key rơi vào cùng mili giây thì bản
# sau ghi đè bản trước. Khi thiết bị vừa kết nối, CONNECTED, ACTIVE và BOOT đến cách nhau
# 0..30 ms nên trước đây thỉnh thoảng mất một dòng nhật ký.
SYS_EVENT_KEY = "sysEvent"

# Tên quan hệ (relation) đầu ra của node "Message Type Switch" (ThingsBoard 4.3)
EVENT_RELATIONS = [
    "Connect Event",          # CONNECT_EVENT      -> "CONNECTED"
    "Disconnect Event",       # DISCONNECT_EVENT   -> "DISCONNECTED"
    "Activity Event",         # ACTIVITY_EVENT     -> "ACTIVE"
    "Inactivity Event",       # INACTIVITY_EVENT   -> "INACTIVE"
    "RPC Request to Device",  # RPC_CALL_FROM_SERVER_TO_DEVICE -> "RPC <method>(<params>)"
]

SWITCH_NODE_TYPE = "org.thingsboard.rule.engine.filter.TbMsgTypeSwitchNode"
SCRIPT_NODE_TYPE = "org.thingsboard.rule.engine.transform.TbTransformMsgNode"
TIMESERIES_NODE_TYPE = "org.thingsboard.rule.engine.telemetry.TbMsgTimeseriesNode"
RPC_CALL_NODE_TYPE = "org.thingsboard.rule.engine.rpc.TbSendRPCRequestNode"

EVENT_NODE_NAME = RULE_NODE_PREFIX + "Event to telemetry"
RPC_OK_NODE_NAME = RULE_NODE_PREFIX + "RPC OK to telemetry"
RPC_FAILED_NODE_NAME = RULE_NODE_PREFIX + "RPC failed to telemetry"
SAVE_NODE_NAME = RULE_NODE_PREFIX + "Save event"
# Đầu ra của node "RPC Call Request" (gửi lệnh tới thiết bị), mỗi đầu ra vào một node script
# riêng: Success = thiết bị đã trả lời (two-way) / đã nhận lệnh (one-way), Failure = lỗi
# (thiết bị offline, quá thời gian chờ...)
RPC_RESULT_NODES = {RPC_OK_NODE_NAME: "Success", RPC_FAILED_NODE_NAME: "Failure"}

# Script đổi sự kiện thành bản tin telemetry {"ts": ..., "values": {"sysEvent": "..."}}.
# Node "save time series" chỉ nhận bản tin loại POST_TELEMETRY_REQUEST nên phải đổi msgType.
# ts = thời điểm rule chain xử lý, làm tròn xuống bội số 10 ms rồi cộng một "ô" riêng cho
# từng loại sự kiện: hai loại khác nhau không bao giờ trùng ts (không ghi đè nhau), trong
# cùng 10 ms vẫn đúng thứ tự:
#   DISCONNECTED +1, INACTIVE +2, CONNECTED +3, ACTIVE +4, RPC (lệnh) +5, RPC (kết quả) +6
# Với RPC từ server (REST /api/rpc/oneway|twoway), msg.params là chuỗi JSON của params:
#   true -> "RPC setLed1(true)", "on" -> "RPC setLed2(\"on\")",
#   {"led":1,"state":true} -> "RPC setLed({\"led\":1,\"state\":true})", không có -> "RPC getLeds()"
EVENT_SCRIPT_TBEL = """\
var text = msgType;
var slot = 0;
if (msgType == "DISCONNECT_EVENT") {
    text = "DISCONNECTED";
    slot = 1;
} else if (msgType == "INACTIVITY_EVENT") {
    text = "INACTIVE";
    slot = 2;
} else if (msgType == "CONNECT_EVENT") {
    text = "CONNECTED";
    slot = 3;
} else if (msgType == "ACTIVITY_EVENT") {
    text = "ACTIVE";
    slot = 4;
} else if (msgType == "RPC_CALL_FROM_SERVER_TO_DEVICE") {
    var params = msg.params;
    if (params == null || params == "null") {
        params = "";
    }
    text = "RPC " + msg.method + "(" + params + ")";
    slot = 5;
}
var now = Date.now();
var ts = now - (now % 10) + slot;
return {msg: {ts: ts, values: {sysEvent: text}}, metadata: metadata, msgType: "POST_TELEMETRY_REQUEST"};"""

# Kết quả lệnh RPC, msg = nội dung thiết bị trả lời (Success) hoặc {"error": "<lỗi>"} (Failure):
#   Success, RPC two-way (REST /api/rpc/twoway) -> "RPC OK: true",
#            "RPC OK: {\"led1\":false,\"led2\":true}",
#            "RPC OK: {\"error\":\"unknown method\"}" (thiết bị trả lời là không biết lệnh)
#   Success, RPC one-way (nút gạt LED trên dashboard, REST /api/rpc/oneway): không có nội dung
#            trả lời, ThingsBoard chỉ báo đã gửi tới thiết bị -> "RPC DELIVERED"
#   Failure -> "RPC FAILED: NO_ACTIVE_CONNECTION" (thiết bị offline), "RPC FAILED: TIMEOUT"
RPC_OK_SCRIPT_TBEL = """\
var text = "RPC OK: " + JSON.stringify(msg);
if (metadata.oneway == "true") {
    text = "RPC DELIVERED";
}
var now = Date.now();
var ts = now - (now % 10) + 6;
return {msg: {ts: ts, values: {sysEvent: text}}, metadata: metadata, msgType: "POST_TELEMETRY_REQUEST"};"""

RPC_FAILED_SCRIPT_TBEL = """\
var reason = JSON.stringify(msg);
if (isMap(msg) && msg.error != null) {
    reason = msg.error;
}
var now = Date.now();
var ts = now - (now % 10) + 6;
return {msg: {ts: ts, values: {sysEvent: "RPC FAILED: " + reason}}, metadata: metadata,
        msgType: "POST_TELEMETRY_REQUEST"};"""

# Bản JavaScript tương đương (chỉ dùng khi đổi scriptLang sang JS trên giao diện)
EVENT_SCRIPT_JS = """\
var names = {DISCONNECT_EVENT: ["DISCONNECTED", 1], INACTIVITY_EVENT: ["INACTIVE", 2],
             CONNECT_EVENT: ["CONNECTED", 3], ACTIVITY_EVENT: ["ACTIVE", 4]};
var item = names[msgType] || [msgType, 0];
var text = item[0], slot = item[1];
if (msgType === "RPC_CALL_FROM_SERVER_TO_DEVICE") {
    var params = msg.params;
    if (params === undefined || params === null || params === "null") { params = ""; }
    if (typeof params !== "string") { params = JSON.stringify(params); }
    text = "RPC " + msg.method + "(" + params + ")";
    slot = 5;
}
var now = Date.now();
var ts = now - (now % 10) + slot;
return {msg: {ts: ts, values: {sysEvent: text}}, metadata: metadata, msgType: "POST_TELEMETRY_REQUEST"};"""

RPC_OK_SCRIPT_JS = """\
var text = metadata.oneway === "true" ? "RPC DELIVERED" : "RPC OK: " + JSON.stringify(msg);
var now = Date.now();
var ts = now - (now % 10) + 6;
return {msg: {ts: ts, values: {sysEvent: text}}, metadata: metadata, msgType: "POST_TELEMETRY_REQUEST"};"""

RPC_FAILED_SCRIPT_JS = """\
var reason = (msg && typeof msg === "object" && msg.error) ? msg.error : JSON.stringify(msg);
var now = Date.now();
var ts = now - (now % 10) + 6;
return {msg: {ts: ts, values: {sysEvent: "RPC FAILED: " + reason}}, metadata: metadata,
        msgType: "POST_TELEMETRY_REQUEST"};"""

# Cấu hình mong muốn của các node Lab4 (dùng cả để tạo lẫn để kiểm tra khi chạy lại)
SCRIPT_CONFIGS = {
    EVENT_NODE_NAME: {"scriptLang": "TBEL", "tbelScript": EVENT_SCRIPT_TBEL, "jsScript": EVENT_SCRIPT_JS},
    RPC_OK_NODE_NAME: {"scriptLang": "TBEL", "tbelScript": RPC_OK_SCRIPT_TBEL, "jsScript": RPC_OK_SCRIPT_JS},
    RPC_FAILED_NODE_NAME: {"scriptLang": "TBEL", "tbelScript": RPC_FAILED_SCRIPT_TBEL,
                           "jsScript": RPC_FAILED_SCRIPT_JS},
}
# useServerTs = False: dùng ts do script đặt (không phải giờ lúc lưu)
SAVE_NODE_CONFIG = {"defaultTTL": 0, "useServerTs": False, "processingSettings": {"type": "ON_EVERY_MESSAGE"}}


def script_node(name, layout_x, layout_y, description):
    return {
        "type": SCRIPT_NODE_TYPE,
        "name": name,
        "configurationVersion": 0,
        "configuration": copy.deepcopy(SCRIPT_CONFIGS[name]),
        "additionalInfo": {"layoutX": layout_x, "layoutY": layout_y, "description": description},
    }


def desired_lab4_nodes(event_xy, rpc_xy):
    """Các rule node cần thêm: script đổi sự kiện -> telemetry, 2 script kết quả RPC (nếu có node
    RPC Call Request ở vị trí rpc_xy), rồi một node lưu time series dùng chung."""
    nodes = [script_node(EVENT_NODE_NAME, *event_xy,
                         "Lab 4: sự kiện kết nối/hoạt động và lệnh RPC -> telemetry "
                         f"\"{SYS_EVENT_KEY}\" (nhật ký hệ thống)")]
    if rpc_xy:
        rpc_x, rpc_y = rpc_xy
        nodes.append(script_node(RPC_OK_NODE_NAME, rpc_x + 300, rpc_y - 68,
                                 f"Lab 4: thiết bị đã trả lời lệnh RPC -> telemetry \"{SYS_EVENT_KEY}\""))
        nodes.append(script_node(RPC_FAILED_NODE_NAME, rpc_x + 600, rpc_y,
                                 f"Lab 4: lệnh RPC lỗi (offline, quá thời gian) -> telemetry \"{SYS_EVENT_KEY}\""))
    nodes.append({
        "type": TIMESERIES_NODE_TYPE,
        "name": SAVE_NODE_NAME,
        "configurationVersion": 1,
        "configuration": copy.deepcopy(SAVE_NODE_CONFIG),
        "additionalInfo": {"layoutX": event_xy[0] + 300, "layoutY": event_xy[1],
                           "description": f"Lab 4: lưu telemetry \"{SYS_EVENT_KEY}\" (ts do script đặt)"},
    })
    return nodes


def connection_signature(nodes, connections):
    """Tập (tên node nguồn, tên node đích, relation) của các kết nối dính tới node Lab4."""
    result = set()
    for conn in connections:
        source, target = nodes[conn["fromIndex"]]["name"], nodes[conn["toIndex"]]["name"]
        if source.startswith(RULE_NODE_PREFIX) or target.startswith(RULE_NODE_PREFIX):
            result.add((source, target, conn["type"]))
    return result


def node_up_to_date(existing, desired):
    """So sánh node Lab4 đang có với bản mong muốn (loại node + các trường cấu hình quan trọng)."""
    if existing is None or existing["type"] != desired["type"]:
        return False
    current = existing.get("configuration") or {}
    return all(current.get(key) == value for key, value in desired["configuration"].items())


def build_rule_chain_metadata(metadata):
    """Trả về (metadata mới, đã_đúng). Bỏ các node "Lab4 ..." cũ rồi thêm lại bản mới,
    giữ nguyên mọi node/kết nối khác và đánh lại chỉ số (fromIndex/toIndex/firstNodeIndex)."""
    nodes = metadata.get("nodes") or []
    connections = metadata.get("connections") or []

    switch_index = next((i for i, n in enumerate(nodes)
                         if n["type"] == SWITCH_NODE_TYPE and n["name"] == "Message Type Switch"), None)
    if switch_index is None:
        switch_index = next((i for i, n in enumerate(nodes) if n["type"] == SWITCH_NODE_TYPE), None)
    if switch_index is None:
        raise SystemExit("Root Rule Chain không có node Message Type Switch, không thể bổ sung.")
    switch_name = nodes[switch_index]["name"]

    # Node "RPC Call Request" nhận lệnh RPC từ switch (không có thì bỏ qua phần ghi kết quả RPC)
    rpc_candidates = [i for i, n in enumerate(nodes) if n["type"] == RPC_CALL_NODE_TYPE]
    rpc_index = next((c["toIndex"] for c in connections
                      if c["fromIndex"] == switch_index and c["type"] == "RPC Request to Device"
                      and c["toIndex"] in rpc_candidates), rpc_candidates[0] if rpc_candidates else None)

    # Vị trí vẽ: cột các node nhận từ switch, ngay dưới node thấp nhất; các node kết quả RPC
    # đặt bên phải node RPC Call Request
    others = [n for n in nodes if not n["name"].startswith(RULE_NODE_PREFIX)]
    targets = [nodes[c["toIndex"]] for c in connections if c["fromIndex"] == switch_index
               and not nodes[c["toIndex"]]["name"].startswith(RULE_NODE_PREFIX)]
    layout_x = max((n.get("additionalInfo") or {}).get("layoutX", 825) for n in targets) if targets else 825
    layout_y = max(((n.get("additionalInfo") or {}).get("layoutY", 0) for n in others), default=0) + 112
    rpc_xy = None
    if rpc_index is not None:
        info = nodes[rpc_index].get("additionalInfo") or {}
        rpc_xy = (info.get("layoutX", layout_x), info.get("layoutY", layout_y - 112))
    new_nodes = desired_lab4_nodes((layout_x, layout_y), rpc_xy)

    wanted_connections = {(switch_name, EVENT_NODE_NAME, rel) for rel in EVENT_RELATIONS}
    wanted_connections.add((EVENT_NODE_NAME, SAVE_NODE_NAME, "Success"))
    if rpc_index is not None:
        rpc_name = nodes[rpc_index]["name"]
        for name, rel in RPC_RESULT_NODES.items():
            wanted_connections.add((rpc_name, name, rel))
            wanted_connections.add((name, SAVE_NODE_NAME, "Success"))

    # Đã đúng chưa? (cùng tên + loại node, cùng script / cấu hình lưu, cùng tập kết nối)
    existing = {n["name"]: n for n in nodes if n["name"].startswith(RULE_NODE_PREFIX)}
    up_to_date = (
        set(existing) == {n["name"] for n in new_nodes}
        and all(node_up_to_date(existing.get(n["name"]), n) for n in new_nodes)
        and connection_signature(nodes, connections) == wanted_connections
    )
    if up_to_date:
        return metadata, True

    # Giữ các node không phải Lab4, ánh xạ chỉ số cũ -> mới
    index_map, kept = {}, []
    for old_index, node in enumerate(nodes):
        if not node["name"].startswith(RULE_NODE_PREFIX):
            index_map[old_index] = len(kept)
            kept.append(node)
    kept_connections = [
        {**conn, "fromIndex": index_map[conn["fromIndex"]], "toIndex": index_map[conn["toIndex"]]}
        for conn in connections
        if conn["fromIndex"] in index_map and conn["toIndex"] in index_map
    ]
    new_index = {node["name"]: len(kept) + i for i, node in enumerate(new_nodes)}
    new_switch_index = index_map[switch_index]
    kept_connections += [{"fromIndex": new_switch_index, "toIndex": new_index[EVENT_NODE_NAME], "type": rel}
                         for rel in EVENT_RELATIONS]
    kept_connections.append({"fromIndex": new_index[EVENT_NODE_NAME], "toIndex": new_index[SAVE_NODE_NAME],
                             "type": "Success"})
    if rpc_index is not None:
        for name, rel in RPC_RESULT_NODES.items():
            kept_connections.append({"fromIndex": index_map[rpc_index], "toIndex": new_index[name], "type": rel})
            kept_connections.append({"fromIndex": new_index[name], "toIndex": new_index[SAVE_NODE_NAME],
                                     "type": "Success"})

    result = copy.deepcopy(metadata)
    result["nodes"] = kept + new_nodes
    result["connections"] = kept_connections
    first = metadata.get("firstNodeIndex")
    result["firstNodeIndex"] = index_map.get(first, new_switch_index) if first is not None else new_switch_index
    return result, False


def describe_connections(metadata):
    nodes = metadata["nodes"]
    return sorted(f"{nodes[c['fromIndex']]['name']} --[{c['type']}]--> {nodes[c['toIndex']]['name']}"
                  for c in metadata.get("connections") or [])


def step_rule_chain(ctx):
    api = ctx.tenant_api
    root = api.find_in_pages("/api/ruleChains", lambda r: r.get("root"), params={"type": "CORE"})
    if root is None:
        raise SystemExit("Không tìm thấy Root Rule Chain của tenant.")
    ctx.root_rule_chain_id = root["id"]["id"]
    metadata = api.get(f"/api/ruleChain/{ctx.root_rule_chain_id}/metadata")
    new_metadata, up_to_date = build_rule_chain_metadata(metadata)
    lab4_names = " / ".join(n["name"] for n in new_metadata["nodes"] if n["name"].startswith(RULE_NODE_PREFIX))
    if RPC_OK_NODE_NAME not in lab4_names:
        print("  [!] Không thấy node RPC Call Request: nhật ký sẽ không có dòng kết quả lệnh RPC")
    if up_to_date:
        ctx.unchanged(f"\"{root['name']}\" đã có node {lab4_names}")
        return

    api.post("/api/ruleChain/metadata", json=new_metadata)
    saved = api.get(f"/api/ruleChain/{ctx.root_rule_chain_id}/metadata")
    _, ok = build_rule_chain_metadata(saved)
    if not ok:
        raise SystemExit("Đã lưu Root Rule Chain nhưng kiểm tra lại không khớp, xem trên giao diện web.")
    ctx.changed(f"Bổ sung \"{root['name']}\": {lab4_names}")
    for line in describe_connections(saved):
        print(f"      {line}")


# ----------------------------------------------------------------------------
# Bước 5: widgets bundle (widget tự viết, file widgets/*.json)
# ----------------------------------------------------------------------------
# Các trường của widget type / bundle được so sánh để quyết định có cần cập nhật không
WIDGET_TYPE_FIELDS = ("name", "description", "tags", "deprecated", "scada", "image", "descriptor")
BUNDLE_FIELDS = ("alias", "title", "description", "order", "image", "scada")


def load_json(path):
    with open(path, encoding="utf-8") as file:
        return json.load(file)


def same_fields(current, desired, fields):
    """True nếu current có cùng giá trị với desired ở mọi trường trong fields (thiếu = None)."""
    return all(current.get(field) == desired.get(field) for field in fields)


def comparable_widget_type(widget_type):
    """Bản sao để so sánh: defaultConfig (chuỗi JSON, server tự định dạng lại) -> object."""
    result = {field: copy.deepcopy(widget_type.get(field)) for field in WIDGET_TYPE_FIELDS}
    descriptor = result.get("descriptor") or {}
    if isinstance(descriptor.get("defaultConfig"), str):
        try:
            descriptor["defaultConfig"] = json.loads(descriptor["defaultConfig"])
        except ValueError:
            pass
    return result


def ensure_widget_type(ctx, desired):
    """Tạo / cập nhật widget type của tenant theo fqn."""
    api = ctx.tenant_api
    fqn = desired["fqn"]
    # Tra cứu theo fqn đầy đủ: "tenant.<fqn>" (widget của tenant), "system.<fqn>" (widget hệ thống)
    existing = api.get("/api/widgetType", params={"fqn": "tenant." + fqn}, allow_404=True)
    if existing:
        # Bản đầy đủ (có description, tags, image)
        existing = api.get(f"/api/widgetType/{existing['id']['id']}")
    if existing and comparable_widget_type(existing) == comparable_widget_type(desired):
        ctx.unchanged(f"Widget type {fqn} đã đúng")
        return
    body = {field: desired.get(field) for field in WIDGET_TYPE_FIELDS}
    body["fqn"] = fqn
    if existing:
        body.update({"id": existing["id"], "version": existing.get("version")})
    api.post("/api/widgetType", json=body)
    ctx.changed(f"{'Cập nhật' if existing else 'Tạo'} widget type {fqn} ({desired['name']})")


def ensure_widgets_bundle(ctx, data, file_name):
    api = ctx.tenant_api
    desired = data["widgetsBundle"]
    title, alias = desired["title"], desired.get("alias")
    matches = api.find_all_in_pages(
        "/api/widgetsBundles",
        lambda b: (b.get("tenantId") or {}).get("id") == ctx.tenant_id
        and (b.get("alias") == alias or b.get("title") == title),
        params={"tenantOnly": "true"})
    if len(matches) > 1:
        print(f"  [!] Có {len(matches)} widgets bundle trùng \"{title}\", chỉ cập nhật bản đầu tiên "
              "(xóa bản thừa trên web: Resources > Widgets library)")
    bundle = matches[0] if matches else None
    if bundle is None:
        bundle = api.post("/api/widgetsBundle", json={field: desired.get(field) for field in BUNDLE_FIELDS})
        ctx.changed(f"Tạo widgets bundle \"{title}\" ({file_name})")
    elif not same_fields(bundle, desired, BUNDLE_FIELDS):
        bundle.update({field: desired.get(field) for field in BUNDLE_FIELDS})
        bundle = api.post("/api/widgetsBundle", json=bundle)
        ctx.changed(f"Cập nhật thông tin widgets bundle \"{title}\"")
    else:
        ctx.unchanged(f"Widgets bundle \"{title}\" đã có")
    bundle_id = bundle["id"]["id"]

    for widget_type in data.get("widgetTypes", []):
        ensure_widget_type(ctx, widget_type)

    # Danh sách widget trong bundle (đúng thứ tự trong file)
    wanted = [w["fqn"] for w in data.get("widgetTypes", [])]
    current = api.get("/api/widgetTypeFqns", params={"widgetsBundleId": bundle_id}) or []
    if list(current) != wanted:
        api.post(f"/api/widgetsBundle/{bundle_id}/widgetTypeFqns", json=wanted)
        ctx.changed(f"Gán {len(wanted)} widget vào bundle \"{title}\": {', '.join(wanted)}")
    else:
        ctx.unchanged(f"Bundle \"{title}\" đủ {len(wanted)} widget")
    ctx.summary_extra.append(f"Widgets bundle    : {title} ({len(wanted)} widget)")


def step_import_widgets_bundle(ctx):
    files = sorted(glob.glob(os.path.join(WIDGETS_DIR, "*.json")))
    if not files:
        ctx.unchanged(f"Không có file widgets bundle trong {WIDGETS_DIR}")
        return
    for path in files:
        data = load_json(path)
        if "widgetsBundle" not in data:
            print(f"  [!] Bỏ qua {os.path.basename(path)}: không phải file export widgets bundle")
            continue
        ensure_widgets_bundle(ctx, data, os.path.basename(path))


# ----------------------------------------------------------------------------
# Bước 6: dashboard (file dashboard_lab4.json) + home dashboard của tenant
# ----------------------------------------------------------------------------
DASHBOARD_FIELDS = ("title", "image", "mobileHide", "mobileOrder", "configuration")


def check_entity_aliases(ctx, configuration):
    """Đếm số thiết bị mỗi entity alias tìm được (alias lọc theo tên / device profile)."""
    api = ctx.tenant_api
    for alias in (configuration.get("entityAliases") or {}).values():
        count = api.post("/api/entitiesQuery/count", json={"entityFilter": alias["filter"]})
        if count:
            ctx.unchanged(f"Alias \"{alias['alias']}\" -> {count} thiết bị")
        else:
            print(f"  [!] Alias \"{alias['alias']}\" không tìm thấy thiết bị nào "
                  f"(filter {json.dumps(alias['filter'], ensure_ascii=False)})")


def step_import_dashboard(ctx):
    api = ctx.tenant_api
    if not os.path.exists(DASHBOARD_FILE):
        ctx.unchanged(f"Không có {DASHBOARD_FILE}")
        return
    desired = load_json(DASHBOARD_FILE)
    title = desired["title"]
    check_entity_aliases(ctx, desired["configuration"])

    matches = api.find_all_in_pages("/api/tenant/dashboards", lambda d: d.get("title") == title,
                                    params={"textSearch": title})
    if len(matches) > 1:
        print(f"  [!] Có {len(matches)} dashboard tên \"{title}\", chỉ cập nhật bản tạo sớm nhất "
              "(xóa bản thừa trên web: Dashboards)")
        matches.sort(key=lambda d: d.get("createdTime", 0))
    body = {field: desired.get(field) for field in DASHBOARD_FIELDS}
    if not matches:
        dashboard = api.post("/api/dashboard", json=body)
        ctx.changed(f"Tạo dashboard \"{title}\"")
    else:
        dashboard = api.get(f"/api/dashboard/{matches[0]['id']['id']}")
        if same_fields(dashboard, body, DASHBOARD_FIELDS):
            ctx.unchanged(f"Dashboard \"{title}\" đã đúng như {os.path.basename(DASHBOARD_FILE)}")
        else:
            dashboard.update(body)
            dashboard = api.post("/api/dashboard", json=dashboard)
            ctx.changed(f"Cập nhật dashboard \"{title}\" theo {os.path.basename(DASHBOARD_FILE)}")
    dashboard_id = dashboard["id"]["id"]

    if ctx.set_home_dashboard:
        home = api.get("/api/tenant/dashboard/home/info") or {}
        if ((home.get("dashboardId") or {}).get("id") != dashboard_id
                or home.get("hideDashboardToolbar") is not False):
            api.post("/api/tenant/dashboard/home/info", json={
                "dashboardId": entity_id("DASHBOARD", dashboard_id),
                "hideDashboardToolbar": False,        # giữ thanh công cụ (chọn khoảng thời gian)
            })
            ctx.changed("Đặt làm home dashboard của tenant (trang Home sau khi đăng nhập)")
        else:
            ctx.unchanged("Đã là home dashboard của tenant")

    states = desired["configuration"].get("states") or {}
    ctx.summary_extra.append(f"Dashboard         : {title}")
    ctx.summary_extra.append(f"                    {ctx.base_url}/dashboards/{dashboard_id}")
    ctx.summary_extra.append("                    state: " + ", ".join(
        f"{state.get('name')} ({sid})" for sid, state in states.items()))


# ----------------------------------------------------------------------------
# Bước 7: tóm tắt
# ----------------------------------------------------------------------------
def print_summary(ctx):
    cfg = ctx.config
    print("\n" + "=" * 72)
    print("TÓM TẮT")
    print("=" * 72)
    print(f"Giao diện web     : {ctx.base_url}")
    print(f"Tenant            : {cfg['tenant']['title']} (id {ctx.tenant_id})")
    print(f"Đăng nhập         : {cfg['tenant_admin']['email']} / {cfg['tenant_admin']['password']}")
    print(f"MQTT              : {cfg['mqtt_host']}:{cfg['mqtt_port']} (thiết bị thật dùng IP LAN của laptop)")
    print("                    username = access token, password để trống")
    print("                    telemetry  -> v1/devices/me/telemetry")
    print("                    attributes -> v1/devices/me/attributes")
    print("                    RPC        <- v1/devices/me/rpc/request/+")
    print("Thiết bị:")
    for name, info in ctx.devices.items():
        print(f"  - {name:<18} id {info['id']}  token {info['token']}")
        print(f"    {'':<18} {ctx.base_url}/entities/devices/{info['id']}")
    if ctx.root_rule_chain_id:
        print(f"Root Rule Chain   : {ctx.base_url}/rulechains/{ctx.root_rule_chain_id}")
    for line in ctx.summary_extra:
        print(line)
    if ctx.changes:
        print(f"\nĐã thực hiện {ctx.changes} thay đổi.")
    else:
        print("\nKhông có thay đổi nào (mọi thứ đã đúng như config.json và các file JSON).")


# ----------------------------------------------------------------------------
# Danh sách bước: thêm bước mới vào STEPS (theo thứ tự chạy)
# ----------------------------------------------------------------------------
STEPS = [
    ("1. Tenant", step_tenant),
    ("1. Tenant admin", step_tenant_admin),
    ("2. Device profile", step_device_profiles),
    ("3. Device, access token, inactivityTimeout", step_devices),
    ("4. Root Rule Chain (telemetry \"sysEvent\")", step_rule_chain),
    ("5. Widgets bundle (widgets/*.json)", step_import_widgets_bundle),
    ("6. Dashboard (dashboard_lab4.json)", step_import_dashboard),
]


def load_config(path):
    with open(path, encoding="utf-8") as file:
        return json.load(file)


def main():
    # Đặt UTF-8 trước khi in bất cứ gì (kể cả --help) để console Windows hiện tiếng Việt
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", line_buffering=True)
    parser = argparse.ArgumentParser(description="Lab 4: khởi tạo ThingsBoard (tenant, device, rule chain, widget, dashboard)")
    parser.add_argument("--config", default=DEFAULT_CONFIG, help="đường dẫn config.json")
    parser.add_argument("--url", help="URL ThingsBoard (mặc định lấy tb_url trong config.json)")
    parser.add_argument("--no-home-dashboard", action="store_true",
                        help="không đặt dashboard Lab 4 làm home dashboard của tenant")
    args = parser.parse_args()

    config = load_config(args.config)
    ctx = Context(config, args.url or config["tb_url"])
    ctx.set_home_dashboard = not args.no_home_dashboard
    print(f"ThingsBoard: {ctx.base_url}")
    try:
        for title, step in STEPS:
            print(f"\n[{title}]")
            step(ctx)
    except TbApiError as error:
        sys.exit(f"\nLỗi REST API: {error}")
    print_summary(ctx)


if __name__ == "__main__":
    main()
