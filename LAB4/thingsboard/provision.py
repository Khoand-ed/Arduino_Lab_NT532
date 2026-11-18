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
         MQTT thì vẫn "active" dù không gửi telemetry; mất điện / mất Wi-Fi / ngắt kết
         nối thì mới chuyển sang không hoạt động.
       - ThingsBoard kiểm tra trạng thái theo chu kỳ DEFAULT_STATE_CHECK_INTERVAL (mặc
         định 60 s), nên thực tế mất 30..90 s.
  4. Bổ sung Root Rule Chain (giữ nguyên các luồng mặc định): mỗi sự kiện
     CONNECTED / DISCONNECTED / ACTIVE / INACTIVE và mỗi lệnh RPC server -> thiết bị
     (vd "RPC setLed1(true)") được lưu thành telemetry "event" của thiết bị.
     Đây là nguồn dữ liệu cho bảng nhật ký hệ thống trên dashboard.
  5. In tóm tắt: URL, tài khoản đăng nhập, id và access token của thiết bị.

Chạy lại nhiều lần vẫn an toàn (idempotent): phần nào đã có thì bỏ qua, chỉ sửa
phần khác với config.json. Lần chạy thứ hai sẽ báo "không có thay đổi".

Cài thư viện:  pip install -r requirements.txt
Chạy:          python provision.py
               python provision.py --url http://localhost:8080 --config config.json
"""

import argparse
import copy
import json
import os
import sys
from urllib.parse import parse_qs, urlparse

import requests

DEFAULT_CONFIG = os.path.join(os.path.dirname(os.path.abspath(__file__)), "config.json")

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
        page = 0
        while True:
            query = dict(params or {}, pageSize=page_size, page=page)
            data = self.get(path, params=query)
            for item in data.get("data", []):
                if predicate(item):
                    return item
            if not data.get("hasNext"):
                return None
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
# Bước 4: Root Rule Chain -> telemetry "event" (nhật ký hệ thống)
# ----------------------------------------------------------------------------
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

EVENT_NODE_NAME = RULE_NODE_PREFIX + "Event to telemetry"
SAVE_NODE_NAME = RULE_NODE_PREFIX + "Save event"

# TBEL: đổi sự kiện thành bản tin telemetry {"event": "..."}. Node "save time series"
# chỉ nhận bản tin loại POST_TELEMETRY_REQUEST nên phải đổi msgType.
# Với RPC từ server (REST /api/rpc/oneway|twoway), msg.params là chuỗi JSON của params:
#   true -> "RPC setLed1(true)", "on" -> "RPC setLed2(\"on\")",
#   {"led":1,"state":true} -> "RPC setLed({\"led\":1,\"state\":true})", không có -> "RPC getLeds()"
EVENT_SCRIPT_TBEL = """\
var event = msgType;
if (msgType == "CONNECT_EVENT") {
    event = "CONNECTED";
} else if (msgType == "DISCONNECT_EVENT") {
    event = "DISCONNECTED";
} else if (msgType == "ACTIVITY_EVENT") {
    event = "ACTIVE";
} else if (msgType == "INACTIVITY_EVENT") {
    event = "INACTIVE";
} else if (msgType == "RPC_CALL_FROM_SERVER_TO_DEVICE") {
    var params = msg.params;
    if (params == null || params == "null") {
        params = "";
    }
    event = "RPC " + msg.method + "(" + params + ")";
}
return {msg: {event: event}, metadata: metadata, msgType: "POST_TELEMETRY_REQUEST"};"""

# Bản JavaScript tương đương (chỉ dùng khi đổi scriptLang sang JS trên giao diện)
EVENT_SCRIPT_JS = """\
var names = {CONNECT_EVENT: "CONNECTED", DISCONNECT_EVENT: "DISCONNECTED",
             ACTIVITY_EVENT: "ACTIVE", INACTIVITY_EVENT: "INACTIVE"};
var event = names[msgType] || msgType;
if (msgType === "RPC_CALL_FROM_SERVER_TO_DEVICE") {
    var params = msg.params;
    if (params === undefined || params === null || params === "null") { params = ""; }
    if (typeof params !== "string") { params = JSON.stringify(params); }
    event = "RPC " + msg.method + "(" + params + ")";
}
return {msg: {event: event}, metadata: metadata, msgType: "POST_TELEMETRY_REQUEST"};"""


def desired_lab4_nodes(layout_x, layout_y):
    """2 rule node cần thêm: script đổi sự kiện -> telemetry, rồi lưu time series."""
    return [
        {
            "type": SCRIPT_NODE_TYPE,
            "name": EVENT_NODE_NAME,
            "configurationVersion": 0,
            "configuration": {"scriptLang": "TBEL", "tbelScript": EVENT_SCRIPT_TBEL,
                              "jsScript": EVENT_SCRIPT_JS},
            "additionalInfo": {"layoutX": layout_x, "layoutY": layout_y,
                               "description": "Lab 4: sự kiện kết nối/hoạt động và lệnh RPC "
                                              "-> telemetry \"event\" (nhật ký hệ thống)"},
        },
        {
            "type": TIMESERIES_NODE_TYPE,
            "name": SAVE_NODE_NAME,
            "configurationVersion": 1,
            "configuration": {"defaultTTL": 0, "useServerTs": True,
                              "processingSettings": {"type": "ON_EVERY_MESSAGE"}},
            "additionalInfo": {"layoutX": layout_x + 300, "layoutY": layout_y,
                               "description": "Lab 4: lưu telemetry \"event\""},
        },
    ]


def connection_signature(nodes, connections):
    """Tập (tên node nguồn, tên node đích, relation) của các kết nối dính tới node Lab4."""
    result = set()
    for conn in connections:
        source, target = nodes[conn["fromIndex"]]["name"], nodes[conn["toIndex"]]["name"]
        if source.startswith(RULE_NODE_PREFIX) or target.startswith(RULE_NODE_PREFIX):
            result.add((source, target, conn["type"]))
    return result


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

    # Vị trí vẽ: cột các node nhận từ switch, ngay dưới node thấp nhất
    others = [n for n in nodes if not n["name"].startswith(RULE_NODE_PREFIX)]
    targets = [nodes[c["toIndex"]] for c in connections if c["fromIndex"] == switch_index]
    layout_x = max((n.get("additionalInfo") or {}).get("layoutX", 825) for n in targets) if targets else 825
    layout_y = max(((n.get("additionalInfo") or {}).get("layoutY", 0) for n in others), default=0) + 112
    new_nodes = desired_lab4_nodes(layout_x, layout_y)
    wanted_connections = {(switch_name, EVENT_NODE_NAME, rel) for rel in EVENT_RELATIONS}
    wanted_connections.add((EVENT_NODE_NAME, SAVE_NODE_NAME, "Success"))

    # Đã đúng chưa? (cùng tên + loại node, cùng script, cùng tập kết nối)
    existing = {n["name"]: n for n in nodes if n["name"].startswith(RULE_NODE_PREFIX)}
    up_to_date = (
        set(existing) == {n["name"] for n in new_nodes}
        and all(existing[n["name"]]["type"] == n["type"] for n in new_nodes)
        and (existing[EVENT_NODE_NAME].get("configuration") or {}).get("tbelScript") == EVENT_SCRIPT_TBEL
        and (existing[EVENT_NODE_NAME].get("configuration") or {}).get("scriptLang") == "TBEL"
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
    event_index, save_index = len(kept), len(kept) + 1
    new_switch_index = index_map[switch_index]
    kept_connections += [{"fromIndex": new_switch_index, "toIndex": event_index, "type": rel}
                         for rel in EVENT_RELATIONS]
    kept_connections.append({"fromIndex": event_index, "toIndex": save_index, "type": "Success"})

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
    if up_to_date:
        ctx.unchanged(f"\"{root['name']}\" đã có node {EVENT_NODE_NAME} / {SAVE_NODE_NAME}")
        return

    api.post("/api/ruleChain/metadata", json=new_metadata)
    saved = api.get(f"/api/ruleChain/{ctx.root_rule_chain_id}/metadata")
    _, ok = build_rule_chain_metadata(saved)
    if not ok:
        raise SystemExit("Đã lưu Root Rule Chain nhưng kiểm tra lại không khớp, xem trên giao diện web.")
    ctx.changed(f"Bổ sung \"{root['name']}\": {EVENT_NODE_NAME} -> {SAVE_NODE_NAME}")
    for line in describe_connections(saved):
        print(f"      {line}")


# ----------------------------------------------------------------------------
# Bước 5: tóm tắt
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
        print("\nKhông có thay đổi nào (mọi thứ đã đúng như config.json).")


# ----------------------------------------------------------------------------
# Danh sách bước: thêm bước mới vào STEPS (theo thứ tự chạy)
# ----------------------------------------------------------------------------
STEPS = [
    ("1. Tenant", step_tenant),
    ("1. Tenant admin", step_tenant_admin),
    ("2. Device profile", step_device_profiles),
    ("3. Device, access token, inactivityTimeout", step_devices),
    ("4. Root Rule Chain (telemetry \"event\")", step_rule_chain),
    # TODO (dashboard): thêm các bước dưới đây, mỗi hàm nhận ctx, dùng ctx.tenant_api,
    # ctx.devices[<tên>]["id"], và ghi thêm dòng tóm tắt vào ctx.summary_extra
    # ("5. Widgets bundle", step_import_widgets_bundle),   # widgets/*.json
    # ("6. Dashboard", step_import_dashboard),             # dashboard_lab4.json
]


def load_config(path):
    with open(path, encoding="utf-8") as file:
        return json.load(file)


def main():
    # Đặt UTF-8 trước khi in bất cứ gì (kể cả --help) để console Windows hiện tiếng Việt
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", line_buffering=True)
    parser = argparse.ArgumentParser(description="Lab 4: khởi tạo ThingsBoard (tenant, device, rule chain)")
    parser.add_argument("--config", default=DEFAULT_CONFIG, help="đường dẫn config.json")
    parser.add_argument("--url", help="URL ThingsBoard (mặc định lấy tb_url trong config.json)")
    args = parser.parse_args()

    config = load_config(args.config)
    ctx = Context(config, args.url or config["tb_url"])
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
