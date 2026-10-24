"""
学生成绩管理系统 —— 后端服务（Python 标准库，零依赖）

作用：把 students.txt 变成一个可以被网页读写的接口（API）。

启动：双击 server/启动服务.bat  （或者在终端里 python server/server.py）
然后浏览器打开：http://localhost:8000

提供的接口：
    GET    /api/students        取全部学生
    POST   /api/students        添加一个学生
    PUT    /api/students/<学号>  修改一个学生（姓名、成绩）
    DELETE /api/students/<学号>  删除一个学生

数据还是存在项目根目录的 students.txt 里，
所以 C++ 程序和这个后端用的是同一份数据，可以互相对着改。
"""

import json
import os
import posixpath
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, unquote

# ---------- 路径配置 ----------
SERVER_DIR = os.path.dirname(os.path.abspath(__file__))
BASE_DIR = os.path.dirname(SERVER_DIR)          # 项目根目录
WEB_DIR = os.path.join(BASE_DIR, "web")         # 前端文件所在目录
DATA_FILE = os.path.join(BASE_DIR, "students.txt")

SUBJECTS = ["数学", "英语", "C++"]
# 默认 8000；设置环境变量 PORT 可以换端口（比如 8000 被别的程序占用时）
PORT = int(os.environ.get("PORT", "8000"))

# 服务是多线程的：两个请求可能同时读-改-写 students.txt，
# 不加锁就会互相覆盖（A 读到旧列表，B 也读到旧列表，各自写完，后写的把先写的吞掉）。
DATA_LOCK = threading.Lock()


# ---------- 数据读写 ----------
def load_students():
    """从 students.txt 读出所有学生，返回列表。"""
    result = []
    if not os.path.exists(DATA_FILE):
        return result
    with open(DATA_FILE, "r", encoding="utf-8") as f:
        for line in f:
            parts = line.split()
            if len(parts) != 2 + len(SUBJECTS):
                continue                      # 格式不对的行直接跳过
            sid, name = parts[0], parts[1]
            try:
                scores = [float(x) for x in parts[2:]]
            except ValueError:
                continue
            result.append({"id": sid, "name": name, "scores": scores})
    return result


def save_students(students):
    """把整个列表写回 students.txt，格式和 C++ 程序完全一致。

    先写临时文件再 os.replace 原子替换：万一写到一半断电/崩溃，
    原来的 students.txt 还是完整的，不会变成半截文件把所有数据弄丢。
    """
    tmp = DATA_FILE + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        for s in students:
            scores = " ".join(format_num(v) for v in s["scores"])
            f.write(f'{s["id"]} {s["name"]} {scores}\n')
    os.replace(tmp, DATA_FILE)

    # 顺手把静态数据也刷新一遍：网页上改完，推到 GitHub Pages 的版本才是新的。
    # 不刷的话线上那份会一直是老数据，别人打开你的链接看到的是旧的。
    write_static_data(students)


def write_static_data(students):
    """生成 web/data.js，格式和 C++ 程序导出的完全一致（谁先跑都不会互相打架）。"""
    path = os.path.join(WEB_DIR, "data.js")
    lines = ["// 本文件由程序自动生成，请勿手改", "var STUDENTS = ["]
    for i, s in enumerate(students):
        scores = ", ".join(format_num(v) for v in s["scores"])
        line = f'  {{ id: "{s["id"]}", name: "{s["name"]}", scores: [{scores}] }}'
        lines.append(line + ("," if i < len(students) - 1 else ""))
    lines.append("];")
    subs = ", ".join(f'"{x}"' for x in SUBJECTS)
    lines.append(f"var SUBJECTS = [{subs}];")

    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    os.replace(tmp, path)


def parse_scores(raw):
    """把请求里的成绩清洗成 float 列表；不合法就抛 ValueError，消息直接给用户看。

    这一步是「不信任客户端」的落地：浏览器可以随便伪造请求，
    分数是不是数字、在不在 0~100、个数对不对，全得服务端自己再查一遍。
    """
    if not isinstance(raw, list) or len(raw) != len(SUBJECTS):
        raise ValueError(f"成绩必须是 {len(SUBJECTS)} 个数字")

    result = []
    for x in raw:
        # 布尔值在 Python 里是 int 的子类（True == 1），要单独挡掉
        if isinstance(x, bool) or not isinstance(x, (int, float, str)):
            raise ValueError("成绩必须是数字")
        try:
            v = float(x)
        except (TypeError, ValueError):
            raise ValueError("成绩必须是数字")
        # NaN / 无穷大是 float() 接受的，但存进文件会把后续计算全毁掉
        if v != v or v in (float("inf"), float("-inf")):
            raise ValueError("成绩必须是有效数字")
        if v < 0 or v > 100:
            raise ValueError(f"成绩必须在 0 到 100 之间，收到 {format_num(v)}")
        result.append(v)
    return result


def format_num(v):
    """成绩是整数就不要写成 88.0，保持和 C++ 输出一致。"""
    return str(int(v)) if float(v) == int(v) else str(v)


class Handler(BaseHTTPRequestHandler):

    # ---------- 兜底：任何没预料到的异常都变成 JSON 500 ----------
    def handle_one_request(self):
        # 不做这层兜底的话，一个没处理的异常会让连接被直接掐断，
        # 浏览器只看到 "Failed to fetch"，前端拿不到任何错误信息。
        try:
            super().handle_one_request()
        except Exception as e:
            try:
                self.err(500, f"服务器内部错误：{e}")
            except Exception:
                pass


    # ---------- 工具方法 ----------
    def send_json(self, obj, code=200):
        body = json.dumps(obj, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def read_json(self):
        length = int(self.headers.get("Content-Length", 0))
        if length == 0:
            return None
        try:
            return json.loads(self.rfile.read(length).decode("utf-8"))
        except (ValueError, UnicodeDecodeError):
            return None

    def err(self, code, msg):
        self.send_json({"error": msg}, code)

    def quiet_log(self):
        # 默认的日志太吵，只打一行简短的
        print(f"  {self.command} {self.path}")

    # ---------- GET：取数据 / 发静态文件 ----------
    def do_GET(self):
        path = urlparse(self.path).path

        if path == "/api/students":
            self.quiet_log()
            self.send_json({"subjects": SUBJECTS, "students": load_students()})
            return

        if path.startswith("/api/"):
            self.err(404, "接口不存在")
            return

        self.serve_static(path)

    def serve_static(self, path):
        # 默认打开首页
        if path == "/" or path == "":
            path = "/index.html"

        # 防止用 ../../ 跳出 web 目录读别的文件
        safe = posixpath.normpath(unquote(path)).lstrip("/")
        full = os.path.join(WEB_DIR, safe)
        if not os.path.abspath(full).startswith(os.path.abspath(WEB_DIR)):
            self.err(403, "禁止访问")
            return
        if not os.path.isfile(full):
            self.err(404, "文件不存在")
            return

        ext = os.path.splitext(full)[1].lower()
        types = {
            ".html": "text/html; charset=utf-8",
            ".js": "application/javascript; charset=utf-8",
            ".css": "text/css; charset=utf-8",
            ".json": "application/json; charset=utf-8",
            ".png": "image/png",
            ".jpg": "image/jpeg",
            ".jpeg": "image/jpeg",
            ".txt": "text/plain; charset=utf-8",
            ".ico": "image/x-icon",
        }
        ctype = types.get(ext, "application/octet-stream")

        with open(full, "rb") as f:
            data = f.read()
        self.send_response(200)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        # 页面和数据必须每次回服务器确认，否则改了代码刷新也看不到效果；
        # 图片可以放心缓存，前端上传后靠 ?v=时间戳主动换地址。
        if ext in (".html", ".js", ".css", ".json"):
            self.send_header("Cache-Control", "no-cache, must-revalidate")
        else:
            self.send_header("Cache-Control", "public, max-age=86400")
        self.end_headers()
        self.wfile.write(data)

    # ---------- POST：添加学生 / 上传照片 ----------
    def do_POST(self):
        path = urlparse(self.path).path

        # 上传照片：POST /api/photos/<学号>，请求体是图片的原始二进制
        if path.startswith("/api/photos/"):
            self.handle_upload(unquote(path[len("/api/photos/"):]))
            return

        if path != "/api/students":
            self.err(404, "接口不存在")
            return

        body = self.read_json()
        if not body:
            self.err(400, "请求体不是合法 JSON")
            return

        sid = str(body.get("id", "")).strip()
        name = str(body.get("name", "")).strip()

        # 数据文件是「空格分隔」的纯文本，学号或姓名里带空格会把一行的列数打乱，
        # 整条记录下次就读不出来了 —— 所以在入口就挡掉。
        if not sid or not name:
            self.err(400, "学号和姓名不能为空")
            return
        if any(ch.isspace() for ch in sid) or any(ch.isspace() for ch in name):
            self.err(400, "学号和姓名里不能有空格")
            return

        try:
            scores = parse_scores(body.get("scores", []))
        except ValueError as e:
            self.err(400, str(e))
            return

        with DATA_LOCK:
            students = load_students()
            if any(s["id"] == sid for s in students):
                self.err(409, f"学号 {sid} 已存在")
                return

            students.append({"id": sid, "name": name, "scores": scores})
            save_students(students)
        self.quiet_log()
        self.send_json({"ok": True, "students": students}, 201)

    # ---------- 照片上传 ----------
    def handle_upload(self, sid):
        # 1. 学生必须存在，否则不知道照片该挂给谁
        students = load_students()
        if not any(s["id"] == sid for s in students):
            self.err(404, f"找不到学号 {sid}")
            return

        # 2. 只收 jpg / png
        ctype = (self.headers.get("Content-Type") or "").lower().split(";")[0].strip()
        ext_map = {"image/jpeg": ".jpg", "image/jpg": ".jpg", "image/png": ".png"}
        if ctype not in ext_map:
            self.err(400, "只支持 jpg 或 png 图片")
            return
        ext = ext_map[ctype]

        # 3. 限制大小，防止传个几十兆的文件把磁盘塞满
        try:
            length = int(self.headers.get("Content-Length", 0))
        except ValueError:
            length = 0
        if length <= 0:
            self.err(400, "没有收到图片数据")
            return
        if length > 3 * 1024 * 1024:
            self.err(413, "图片不能超过 3MB")
            return

        data = self.rfile.read(length)

        # 4. 落盘：先把同一个人旧的 .jpg/.png 删掉，避免新旧两张同时存在
        photos_dir = os.path.join(WEB_DIR, "photos")
        os.makedirs(photos_dir, exist_ok=True)
        for old in (".jpg", ".png"):
            old_path = os.path.join(photos_dir, sid + old)
            if os.path.exists(old_path):
                os.remove(old_path)

        filename = sid + ext
        with open(os.path.join(photos_dir, filename), "wb") as f:
            f.write(data)

        self.quiet_log()
        self.send_json({"ok": True, "file": filename, "size": len(data)}, 201)

    # ---------- PUT：修改 ----------
    def do_PUT(self):
        path = urlparse(self.path).path
        prefix = "/api/students/"
        if not path.startswith(prefix):
            self.err(404, "接口不存在")
            return

        sid = unquote(path[len(prefix):])
        body = self.read_json()
        if not body:
            self.err(400, "请求体不是合法 JSON")
            return

        new_name = str(body["name"]).strip() if "name" in body else ""
        if new_name and any(ch.isspace() for ch in new_name):
            self.err(400, "姓名里不能有空格")
            return

        new_scores = None
        if "scores" in body:
            try:
                new_scores = parse_scores(body["scores"])
            except ValueError as e:
                self.err(400, str(e))
                return

        with DATA_LOCK:
            students = load_students()
            target = next((s for s in students if s["id"] == sid), None)
            if target is None:
                self.err(404, f"找不到学号 {sid}")
                return

            if new_name:
                target["name"] = new_name
            if new_scores is not None:
                target["scores"] = new_scores

            save_students(students)
        self.quiet_log()
        self.send_json({"ok": True, "students": students})

    # ---------- DELETE：删除 ----------
    def do_DELETE(self):
        path = urlparse(self.path).path
        prefix = "/api/students/"
        if not path.startswith(prefix):
            self.err(404, "接口不存在")
            return

        sid = unquote(path[len(prefix):])
        with DATA_LOCK:
            students = load_students()
            remain = [s for s in students if s["id"] != sid]

            if len(remain) == len(students):
                self.err(404, f"找不到学号 {sid}")
                return

            save_students(remain)
        self.quiet_log()
        self.send_json({"ok": True, "students": remain})


class Server(ThreadingHTTPServer):
    """Windows 下必须关掉地址复用。

    默认的 HTTPServer 开了 SO_REUSEADDR，在 Windows 上它的含义是
    「允许别的进程抢同一个端口」——端口被占了也照样"绑定成功"，
    结果两个进程抢一个端口，网页时好时坏，极难排查。
    """
    allow_reuse_address = (os.name != "nt")


def main():
    print("=" * 46)
    print("  学生成绩管理系统 · 后端服务")
    print("=" * 46)
    print(f"  数据文件：{DATA_FILE}")
    print(f"  网页目录：{WEB_DIR}")
    print(f"  打开地址：http://localhost:{PORT}")
    print("  停止服务：按 Ctrl + C")
    print("=" * 46)

    # 启动时把静态数据对齐一次：万一之前是 C++ 改的数据，data.js 也能跟上
    try:
        write_static_data(load_students())
        print("  [已同步] web/data.js")
    except Exception as e:
        print(f"  [警告] 同步 data.js 失败：{e}")

    try:
        server = Server(("127.0.0.1", PORT), Handler)
    except OSError:
        print()
        print("  [错误] 端口 %d 已被占用，通常说明已经有一个服务在跑。" % PORT)
        print("  办法：先找到那个黑窗口按 Ctrl+C 关掉，再重新运行本脚本。")
        print("  或者换端口：set PORT=8001 然后重新运行，浏览器改访问 http://localhost:8001")
        print()
        input("按回车退出...")
        return

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n服务已停止。")


if __name__ == "__main__":
    main()
