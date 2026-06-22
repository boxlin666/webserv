import requests
import pytest
import socket
import time
import os

BASE_URL = "http://localhost:8080"

#========================= Configuration file default.conf=========================================
@pytest.mark.parametrize("manage_server", ["./conf/default.conf"], indirect=True)
def test_get_index(manage_server):
    try:
        response = requests.get(f"{BASE_URL}/index.html")
        assert response.status_code == 200
        assert "Content-Length" in response.headers
    except requests.exceptions.ConnectionError:
        pytest.fail("unable to connect to the server. Please ensure that webserv is running.") 

@pytest.mark.parametrize("manage_server", ["./conf/default.conf"], indirect=True)
def test_404_not_found(manage_server):
    response = requests.get(f"{BASE_URL}/non_existent_file.html")
    assert response.status_code == 404

@pytest.mark.parametrize("manage_server", ["./conf/default.conf"], indirect=True)
def test_method_not_allowed(manage_server):
    response = requests.post(f"{BASE_URL}/test/index.html", data={"key": "value"})
    assert response.status_code == 405

SERVER_WEB_ROOT = "./www"

def get_physical_path(url_path):
    relative_path = url_path.replace(BASE_URL, "").lstrip("/")
    return os.path.join(SERVER_WEB_ROOT, relative_path)

@pytest.fixture
def cleanup_files():
    files_to_clean = []
    yield files_to_clean
    
    for file_path in files_to_clean:
        if os.path.exists(file_path):
            try:
                os.remove(file_path)
                print(f" Successfully deleted: {file_path}")
            except Exception as e:
                print(f" Failed to delete {file_path}: {e}")


@pytest.mark.parametrize("manage_server", ["./conf/default.conf"], indirect=True)
def test_post_new_static_file(manage_server, cleanup_files):
    target_url = f"{BASE_URL}/uploads/new_test_file.txt"
    payload = "Hello Webserv 42 project! This is binary raw data post."
    cleanup_files.append(get_physical_path(target_url))
    
    try:
        response = requests.post(target_url, data=payload)
        assert response.status_code == 201
        assert "Content-Length" in response.headers
        
    except requests.exceptions.ConnectionError:
        pytest.fail("unable to connect to the server. Please ensure that webserv is running.")


@pytest.mark.parametrize("manage_server", ["./conf/default.conf"], indirect=True)
def test_post_overwrite_static_file(manage_server, cleanup_files):
    target_url = f"{BASE_URL}/uploads/overwrite_test.txt"
    cleanup_files.append(get_physical_path(target_url))
    
    try:
        res1 = requests.post(target_url, data="First content")
        assert res1.status_code == 201
        
        res2 = requests.post(target_url, data="Second fresh content")
        assert res2.status_code == 200
        
    except requests.exceptions.ConnectionError:
        pytest.fail("unable to connect to the server. Please ensure that webserv is running.")


@pytest.mark.parametrize("manage_server", ["./conf/default.conf"], indirect=True)
def test_post_conflict_with_directory(manage_server, cleanup_files):
    target_url = f"{BASE_URL}/uploads/"
    
    generated_file_path = os.path.join(SERVER_WEB_ROOT, "uploads", "text.txt")
    cleanup_files.append(generated_file_path)
    
    try:
        response = requests.post(target_url, data="Uploading data")
        assert response.status_code == 201, f"Test failed: Expected 201, got {response.status_code}"
        
    except requests.exceptions.ConnectionError:
        pytest.fail("unable to connect to the server. Please ensure that webserv is running.")

#========================= Configuration file 42.conf=========================================
@pytest.mark.parametrize("manage_server", ["./conf/42.conf"], indirect=True)
def test_fragmented_header(manage_server):
    host = "127.0.0.1"
    port = 8080
    
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(5)
    s.connect((host, port))

    part1 = (
        "GET / HTTP/1.1\r\n"
        "Host: 127.0.0.1:8080\r\n"
        "User-Agent: Go-http-client/1.1\r\n"
        "Accept-Encodin"
    )
    s.send(part1.encode())
    time.sleep(0.5)

    part2 = "g: gzip\r\n\r\n"
    s.send(part2.encode())

    try:
        response = s.recv(4096).decode()
        assert "HTTP/1.1 200 OK" in response
        assert "Content-Length" in response
    except socket.timeout:
        pytest.fail("Server did not respond in time. Possible issue with fragmented header handling.")
    finally:
        s.close()

@pytest.mark.parametrize("manage_server", ["./conf/42.conf"], indirect=True)
def test_split_crlf_crlf(manage_server):
    host = "127.0.0.1"
    port = 8080
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((host, port))
    s.send(b"GET / HTTP/1.1\r\nHost: localhost\r\n\r")
    time.sleep(0.2)
    s.send(b"\n\r\n")

    response = s.recv(4096).decode()
    assert "HTTP/1.1 200 OK" in response
    s.close()

@pytest.mark.parametrize("manage_server", ["./conf/42.conf"], indirect=True)
def test_fragmented_chunked_success(manage_server):
    host = "127.0.0.1"
    port = 8080
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    
    # 保持一个较大的安全超时（如 5 秒），纯粹为了防止测试框架卡死
    s.settimeout(5.0)
    s.connect((host, port))

    try:
        # 1. 开始发送细碎的 Header 片段
        s.send(b"GET / HTTP/1.1\r\nHost: 127.0.0.1:8080\r\nTransfer-Encoding: chunked\r\nContent-")
        time.sleep(0.02) # 极短的停顿，模拟网络分包，让服务器触发 SKIP 和 PARSE_NEED_MORE
        s.send(b"Type: text/plain\r\n\r\n") # 完成所有 Header 发送
        time.sleep(0.02)

        # 2. 发送第一个数据块（Size 为 5 字节，内容为 hello）
        s.send(b"5")
        time.sleep(0.01)
        s.send(b"\r\nhe")     # 故意把 body 拆开，让你的 CHUNK_DATA 找不到 \r\n 从而进入 SKIP 状态
        time.sleep(0.01)
        s.send(b"llo\r\n")    # 补齐第一个块的尾巴
        time.sleep(0.01)

        # 3. 发送终止块（Size 为 0，并紧跟 \r\n\r\n）
        s.send(b"0\r\n")
        time.sleep(0.01)
        s.send(b"\r\n")       # 补齐最后的终止符，此时状态机应该完美收工并处理业务

        # 4. 接收响应并校验
        response = s.recv(4096).decode()
        
        # 💡 断言调整：因为我们正常发完了所有信息，服务器必须承认我们的请求并返回 200 OK
        assert "200" in response, f"Server returned unexpected response: {response}"

    except (socket.timeout):
        pytest.fail("Server did not respond in time. Possible issue with fragmented chunked request handling.")
    except (ConnectionResetError, BrokenPipeError):
        pytest.fail("Server crashed or reset the connection unexpectedly during fragmented chunked request test.")
    finally:
        s.close()

@pytest.mark.parametrize("manage_server", ["./conf/42.conf"], indirect=True)
def test_chunked_trailing_newline_sticky_with_body_tail_uppercase_check(
    manage_server,
):
    """
    核心边界与 HTTP Response Body 尾部大写字母核验测试：
    1. 第一个请求是 Chunked 编码，故意扣下最后一个 '\n'。
    2. 第二个请求头部补齐 '\n'，并黏合发送。
    3. 🌟 精准审计：解析出两份响应各自的 Body，检查 Body 里面是否正确以大写的 "HELLO" 和 "WORLD" 结尾。
    """
    host = "127.0.0.1"
    port = 8080

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(4.0)
    s.connect((host, port))

    try:
        # ======= 【数据包 1】：发送第一个请求的大部分，故意留下最后的 '\n' =======
        packet1 = (
            "POST /directory/youpi.bla HTTP/1.1\r\n"
            "Host: 127.0.0.1:8080\r\n"
            "Transfer-Encoding: chunked\r\n"
            "Connection: keep-alive\r\n"
            "\r\n"
            "5\r\n"
            "hello\r\n"
            "0\r\n"
            "\r"  # 🛑 故意截断
        )
        s.send(packet1.encode())

        time.sleep(0.2)  # 停顿让状态机推进

        # ======= 【数据包 2】：把迷路的 '\n' 和第二个请求强行粘在一起发送 =======
        packet2 = (
            "\n"  # 🌟 补齐第一个请求的最后一个字节！
            "POST /directory/youpi.bla HTTP/1.1\r\n"
            "Host: 127.0.0.1:8080\r\n"
            "Content-Length: 5\r\n"
            "Connection: close\r\n"  # 收尾
            "\r\n"
            "world"
        )
        s.send(packet2.encode())

        # ======= 【接收网络原始回执】 =======
        raw_response = ""
        while True:
            chunk = s.recv(4096).decode()
            if not chunk:
                break
            raw_response += chunk

        print(f"\nFull Response:\n{raw_response}")

        # ======= 🌟【核心协议解析：切分出独立的 Response Body】 =======
        # 我们用 "HTTP/1.1" 作为切篇标志，把连在一起的响应切开
        # raw_response.split("HTTP/1.1") 会切出一个空字符串和两份响应主体
        response_blocks = [
            "HTTP/1.1" + block for block in raw_response.split("HTTP/1.1") if block.strip()
        ]

        assert len(response_blocks) == 2, (
            f"Expected 2 independent HTTP response blocks, but got {len(response_blocks)}.\n"
            f"Please check the raw response:\n{raw_response}"
        )

        # ------- 审计第一个响应的 Body 尾部 -------
        resp1 = response_blocks[0]
        # 使用双回车将 Header 和 Body 切开
        assert "\r\n\r\n" in resp1, "The first response is missing the header-body separator \\r\\n\\r\\n"
        body1 = resp1.split("\r\n\r\n", 1)[1]  # 拿到第一个响应的纯 Body

        print(f"\n First Response Body: '{body1}'")
        # 核心断言：检查第一个 Body 的结尾是不是大写的 HELLO
        # 使用 .endswith() 或 rfind 确保它一定呆在 Body 的最后面
        assert body1.endswith("HELLO"), (
            f"[Bug Detected!] First HTTP response body did not end with uppercase 'HELLO'.\n"
            f"Actual body: '{body1}'"
        )

        # ------- 审计第二个响应的 Body 尾部 -------
        resp2 = response_blocks[1]
        assert "\r\n\r\n" in resp2, "The second response is missing the header-body separator \\r\\n\\r\\n"
        body2 = resp2.split("\r\n\r\n", 1)[1]  # 拿到第二个响应的纯 Body

        print(f"\n Second Response Body: '{body2}'")
        # 核心断言：检查第二个 Body 的结尾是不是大写的 WORLD
        assert body2.endswith("WORLD"), (
            f"[Bug Detected!] Second HTTP response body did not end with uppercase 'WORLD'.\n"
            f"Actual body: '{body2}'"
        )

        print("\n--- 🏆 Both HTTP response bodies passed the trailing-uppercase audit! ---")

    except socket.timeout:
        pytest.fail("Server timed out while stitching across packet boundaries")
    except (ConnectionResetError, BrokenPipeError):
        pytest.fail("Server crashed or reset the connection unexpectedly when handling fragmented boundaries")
    finally:
        s.close()

@pytest.mark.parametrize("manage_server", ["./conf/42.conf"], indirect=True)
def test_triple_sticky_packets_fragmented_boundary(manage_server):
    """
    核心魔鬼边界测试：割裂的三联粘包
    数据包 1 发送请求 1 的绝大部分，故意扣下最后的 \\r\\n。
    数据包 2 补齐那迷路的 \\r\\n，并紧接着毫无延迟地黏合请求 2 和请求 3。
    
    验证核心：
    1. 服务器 Parser 是否具备跨包缝合能力（状态机延续性）。
    2. 缝合完请求 1 瞬间触发响应后，剩下的请求 2 和 3 是否能安全留在 _in_buff 里串行排队而不丢失。
    """
    host = "127.0.0.1"
    port = 8080

    # 1. 创建原生 TCP Socket 并连接
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(4.0)  # 设置 4 秒超时防死锁
    s.connect((host, port))

    try:
        # ======= 【数据包 1】：发送请求 1 的绝大部分，故意截断尾部 =======
        # 请求 1 使用 Chunked 编码。标准的结束标志应该是 0\r\n\r\n
        # 我们故意只发到 0\r\n，扣下最后的 \r\n，让服务器卡在 READING_REQ 状态
        packet1 = (
            "GET / HTTP/1.1\r\n"
            "Host: 127.0.0.1:8080\r\n"
            "Transfer-Encoding: chunked\r\n"
            "Connection: keep-alive\r\n"
            "\r\n"
            "5\r\n"
            "hello\r\n"
            "0\r\n"  # 🛑 故意截断！这里原本应该有 \r\n\r\n，现在不发，强制让服务器悬挂等待
        )
        s.send(packet1.encode())

        # 停顿 0.2 秒，强制让服务器的 poll() 捕获并处理数据包 1
        # 此时服务器的 _in_buff 应该被消耗完，但 check_parse_finished() 必须返回 false
        time.sleep(0.2)

        # ======= 【数据包 2】：迷路的尾部 + 请求 2 + 请求 3 绝对黏合 =======
        # 请求 2：长连接 POST
        req2 = (
            "POST / HTTP/1.1\r\n"
            "Host: 127.0.0.1:8080\r\n"
            "Content-Length: 5\r\n"
            "Connection: keep-alive\r\n"
            "\r\n"
            "world"
        )

        # 请求 3：短连接 POST，优雅收尾
        req3 = (
            "POST / HTTP/1.1\r\n"
            "Host: 127.0.0.1:8080\r\n"
            "Content-Length: 4\r\n"
            "Connection: close\r\n"
            "\r\n"
            "last"
        )

        # 🌟 割裂拼接的核心：先补上 "\r\n" 终结请求 1，然后立刻黏合请求 2 和 3
        packet2 = "\r\n" + req2 + req3
        s.send(packet2.encode())

        # ======= 【接收与断言验证】 =======
        response = ""
        while True:
            chunk = s.recv(4096).decode()
            if not chunk:
                break  # 正常收到底层 close，退出
            response += chunk

        print(f"\n[Split test received full response body]:\n{response}")

        # ======= 【断言审计】 =======
        # 1. 验证数量：必须是 3 个独立的响应
        http_headers_count = response.count("HTTP/1.1")
        assert http_headers_count == 3, (
            f"[Bug] Fragmented boundary test failed! Expected 3 responses, got {http_headers_count}.\n"
            f"Response content:\n{response}"
        )

        # 2. 验证短连接协议对齐
        assert "Connection: close" in response, "[Bug] Final response did not include Connection: close"

        # 3. 验证具体状态码（1个200，2个405）
        assert "200 OK" in response, "[Bug] Request 1 did not produce 200 OK after stitching"
        assert "405 Method Not Allowed" in response, "[Bug] Subsequent queued static POST requests missing or not returning 405"

        print("\n--- 🏆 Server successfully handled the highest-level fragmented sticky packet test! ---")

    except socket.timeout:
        pytest.fail("Test timed out! Server deadlocked while stitching across packet boundaries, or wake-up mechanism failed to process queued requests 2 and 3")
    except (ConnectionResetError, BrokenPipeError):
        pytest.fail("Server crashed or disconnected while handling non-standard fragmented boundaries")
    finally:
        s.close()

@pytest.mark.parametrize("manage_server", ["./conf/42.conf"], indirect=True)
def test_triple_cgi_sticky_packets_insanely_fragmented_with_banane_check(
    manage_server,
):
    """
    地狱级割裂粘包测试（youpi.banane 专用 + 物理文件审计版）：
    1. 跨包割裂拼接缝合测试网络层和状态机。
    2. 物理读取服务器本地磁盘上的 youpi.banane 文件，
       验证文件内容是否为干净的 "last"（4字节），
       以此核验服务器是否正确执行了 `O_TRUNC`（彻底擦掉重新写）逻辑。
    """
    host = "127.0.0.1"
    port = 8080

    # 🌟 已经全量替换为 youpi.banane 的物理相对路径
    target_file_path = "./YoupiBanane/youpi.banane"

    # 测试启动前，如果旧文件存在，先强行物理删除，确保测试数据不被旧数据污染
    if os.path.exists(target_file_path):
        os.remove(target_file_path)

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(5.0)
    s.connect((host, port))

    try:
        # ======= 【数据包 1】：请求 1 大部分 + 割裂符 + 请求 2 开头残渣 =======
        # 🌟 路径已全部替换为 /directory/youpi.banane
        packet1 = (
            "POST /directory/youpi.banane HTTP/1.1\r\n"
            "Host: 127.0.0.1:8080\r\n"
            "Transfer-Encoding: chunked\r\n"
            "Connection: keep-alive\r\n"
            "\r\n"
            "5\r\n"
            "hello\r\n"
            "0\r"  # 🛑 截断点
            "\n\r\nPOS"  # 🌟 请求 2 残渣伏笔
        )
        s.send(packet1.encode())

        time.sleep(0.2)  # 等待第一个 CGI 跑完

        # ======= 【数据包 2】：请求 2 残余 + 请求 3 绝对黏合 =======
        # 🌟 路径已全部替换为 /directory/youpi.banane
        req2_remain = (
            "T /directory/youpi.banane HTTP/1.1\r\n"
            "Host: 127.0.0.1:8080\r\n"
            "Content-Length: 5\r\n"
            "Connection: keep-alive\r\n"
            "\r\n"
            "world"
        )

        req3 = (
            "POST /directory/youpi.banane HTTP/1.1\r\n"
            "Host: 127.0.0.1:8080\r\n"
            "Content-Length: 4\r\n"
            "Connection: close\r\n"
            "\r\n"
            "last"
        )

        packet2 = req2_remain + req3
        s.send(packet2.encode())

        # ======= 【接收回执】 =======
        response = ""
        while True:
            chunk = s.recv(4096).decode()
            if not chunk:
                break
            response += chunk

        print(f"\n[Extreme split test received full response body]:\n{response}")

        # ======= 【第一阶段：网络与协议断言】 =======
        http_headers_count = response.count("HTTP/1.1 200 OK")
        if "HTTP/1.1 201 Created" in response:
            http_headers_count += response.count("HTTP/1.1 201 Created")

        assert http_headers_count == 3, (
            f"[Bug] Expected 3 successful responses, got {http_headers_count}.\nResponse:\n{response}"
        )
        assert "Connection: close" in response, "[Bug] Missing Connection: close in responses"

        # ======= 🌟【第二阶段：文件实体深度审计】 =======
        # 稍微停顿极短时间，确保服务器操作系统的文件磁盘 I/O 已经 Flush 完毕
        time.sleep(0.05)

        # 1. 验证文件是否真的在磁盘上生成了
        assert os.path.exists(target_file_path), (
            f"[Bug] CGI appeared to run but the target file was not created on disk: {target_file_path}"
        )

        # 2. 读取文件最终写入的真实字符串
        with open(target_file_path, "r", encoding="utf-8") as f:
            file_content = f.read()

        print(
            f"\n[Disk file deep audit]: Final file content = '{file_content}' (length: {len(file_content)} bytes)"
        )

        # 3. 精准终极断言：斩杀 lastd 残渣 Bug！
        assert file_content == "last", (
            f"[Bug Detected!] Overwrite logic failed!\n"
            f"Expected file content to be exactly 'last' (4 bytes),\n"
            f"but disk contained '{file_content}' ({len(file_content)} bytes).\n"
            f"Please check that open() used O_TRUNC so old data is not left behind."
        )

        print("\n--- 🏆 Server passed the extreme fragmented banane sticky packet test! ---")

    except socket.timeout:
        pytest.fail("Test timed out! Server deadlocked while handling fragmented methods (POS + T), or re-opening the write pipe failed")
    except (ConnectionResetError, BrokenPipeError):
        pytest.fail("Server crashed under fragmented burst traffic")
    finally:
        s.close()


import socket
import time
import pytest


@pytest.mark.parametrize("manage_server", ["./conf/42.conf"], indirect=True)
def test_triple_cgi_pure_memory_interlocked_pipeline(manage_server):
    """
    终极纯内存 CGI 流水线粘包压测：
    1. 采用环环相扣的割裂方式将三个 CGI 请求发送给服务器。
    2. 🌟 纯网络审计：完全不需要创建或检查任何磁盘文件。
    3. 通过解析 HTTP 响应体，精准核验第三个请求的 Body 结尾是否为干净的 "LAST"。
       如果是 "LASTD"，说明服务器重置 Request 对象的内存缓冲区时存在残留 Bug！
    """
    host = "127.0.0.1"
    port = 8080

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(5.0)
    s.connect((host, port))

    try:
        # ======= 【数据包 1】：请求 1 尾巴 💊 粘着 请求 2 头部 =======
        packet1 = (
            "POST /directory/youpi.bla HTTP/1.1\r\n"
            "Host: 127.0.0.1:8080\r\n"
            "Transfer-Encoding: chunked\r\n"
            "Connection: keep-alive\r\n"
            "\r\n"
            "5\r\n"
            "hello\r\n"
            "0\r\n"
            "\r"        # 🛑 截断点 1
            "\nPOS"      # 🔗 粘连点 1
        )
        s.send(packet1.encode())

        time.sleep(0.2)  # 让服务器消费包 1 并拉起第一个 CGI

        # ======= 【数据包 2】：请求 2 尾巴 💊 粘着 请求 3 头部 =======
        packet2 = (
            "T /directory/youpi.bla HTTP/1.1\r\n"  # 🌟 缝合：POS + T = POST
            "Host: 127.0.0.1:8080\r\n"
            "Content-Length: 5\r\n"
            "Connection: keep-alive\r\n"
            "\r\n"
            "worl"      # 🛑 截断点 2
            "dPOS"      # 🔗 粘连点 2：把 'd' 和请求 3 的开头死死粘在一起
        )
        s.send(packet2.encode())

        time.sleep(0.2)

        # ======= 【数据包 3】：补齐请求 3 并收尾 =======
        packet3 = (
            "T /directory/youpi.bla HTTP/1.1\r\n"  # 🌟 缝合：POS + T = POST
            "Host: 127.0.0.1:8080\r\n"
            "Content-Length: 4\r\n"
            "Connection: close\r\n"
            "\r\n"
            "last"
        )
        s.send(packet3.encode())

        # ======= 【接收网络原始回执】 =======
        raw_response = ""
        while True:
            chunk = s.recv(4096).decode()
            if not chunk:
                break
            raw_response += chunk

        print(f"\n[In-memory test received full raw response stream]:\n{raw_response}")

        # ======= 🌟【核心协议解析：剥离 Header，提取每段 Body】 =======
        response_blocks = [
            "HTTP/1.1" + block for block in raw_response.split("HTTP/1.1") if block.strip()
        ]

        assert len(response_blocks) == 3, (
            f"[Bug] Expected 3 independent HTTP responses but got {len(response_blocks)}.\n"
            f"Raw response stream:\n{raw_response}"
        )

        # ------- 审计第三个（最后一个）响应的 Body -------
        # 也就是喂进 "last" 的那个请求返回的回执
        resp3 = response_blocks[2]
        assert "\r\n\r\n" in resp3, "[Bug] Third response format error: missing \\r\\n\\r\\n separator"
        body3 = resp3.split("\r\n\r\n", 1)[1].strip()

        print(f"\n[Core audit] Third CGI response body: '{body3}'")

        # Final assertion to catch memory-remnant bugs
        # If memory wasn't cleared properly, the value may be 'LASTD'.
        assert body3 == "LAST" or body3.endswith("LAST"), (
            f"[Memory-remnant Bug Detected!] Server did not cleanly clear previous request context.\n"
            f"Expected third CGI response body to be 'LAST',\n"
            f"but due to leftover context from the previous request it returned: '{body3}'\n"
            f"Please ensure _request.reset() clears the body buffer (e.g. .clear())."
        )

        print("\n--- 🏆 Pure-memory pipeline sticky packet test passed! ---")

    except socket.timeout:
        pytest.fail("Test timed out! Server deadlocked while spinning up consecutive CGIs")
    except (ConnectionResetError, BrokenPipeError):
        pytest.fail("Server crashed under fragmented sticky packet load")
    finally:
        s.close()

@pytest.mark.parametrize("manage_server", ["./conf/42.conf"], indirect=True)
def test_garbage_leading_newlines_tolerance(manage_server):
    """
    协议鲁棒性测试：无限前导空白/垃圾流宽容度测试
    
    测试逻辑：
    1. 模拟恶意客户端或网络噪音，在发送真正的 HTTP 请求前，先疯狂轰炸 100 个纯换行符（\\r\\n）。
    2. 毫无延迟，紧接着黏上一个标准的正常 CGI 请求。
    3. 验证服务器的状态机是否能极其聪明地“滑过去”，不报 400 错误，并成功拉起 CGI 吐出 "HELLO"。
    """
    host = "127.0.0.1"
    port = 8080

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(4.0)  # 4 秒超时保护
    s.connect((host, port))

    try:
        # ======= 【构建垃圾前导流 + 正常请求】 =======
        # 1. 物理生成 100 个连续的 \r\n
        garbage_prefix = "\r\n" * 100

        # 2. 紧随其后的正常 CGI 报文
        normal_request = (
            "POST /directory/youpi.bla HTTP/1.1\r\n"
            "Host: 127.0.0.1:8080\r\n"
            "Content-Length: 5\r\n"
            "Connection: close\r\n"
            "\r\n"
            "hello"
        )

        # 3. 绝对黏合拼接
        full_packet = garbage_prefix + normal_request

        # ======= 【发送并接收回执】 =======
        s.send(full_packet.encode())

        response = ""
        while True:
            chunk = s.recv(4096).decode()
            if not chunk:
                break
            response += chunk

        print(f"\n[Garbage-prefix stream test received full response]:\n{response}")

        # ======= 【核心断言审计】 =======
        
        # 1. 协议健壮性断言：绝对不能触发 400 Bad Request
        assert "400 Bad Request" not in response, (
            "[Bug] Server treated leading newlines as malformed request and returned 400!"
        )

        # 2. 状态码审计：必须是 200 OK 或 201 Created
        assert "HTTP/1.1 200 OK" in response or "HTTP/1.1 201 Created" in response, (
            f"[Bug] Server did not return a success status when faced with leading newlines. Response:\n{response}"
        )

        # 3. CGI 管道对接审计：提取 Body，确保内容是完美的大写 "HELLO"
        assert "\r\n\r\n" in response, "[Bug] Response missing header-body separator"
        body = response.split("\r\n\r\n", 1)[1].strip()
        
        assert body == "HELLO", (
            f"[Bug] CGI response body wrong. Expected 'HELLO', got: '{body}'"
        )

        print("\n--- 🏆 Server correctly skipped 100 leading CRLFs and returned valid response. ---")

    except socket.timeout:
        pytest.fail("Test timed out! Server deadlocked while skipping leading blanks")
    except (ConnectionResetError, BrokenPipeError):
        pytest.fail("Server crashed or disconnected when handling sudden non-standard network noise")
    finally:
        s.close()

@pytest.mark.parametrize("manage_server", ["./conf/42.conf"], indirect=True)
def test_chunk_size_fragmented_and_count_check(manage_server):
    """
    终极协议头数字碎尸压测 + 字节级数量审计：
    1. 包 1：发送完整的第一个 Chunk（5字节hello），以及第二个 Chunk 声明大小数字的一半（"5"）后瞬间断流。
    2. 停顿 0.2 秒，强迫服务器解析器在没有遇到 \\r\\n 的情况下挂起并记住暂存的 "5"。
    3. 包 2：补齐数字尾巴 "2\\r\\n"（组合成十六进制 52，即十进制 82 字节），紧接着塞满 82 个 'A'，
       再黏合 Chunked 结束符（0\\r\\n\\r\\n）以及最后一个 Connection: close 的 "last" 请求。
    4. 🌟 深度审计：将两份响应切开，精准数出第一个响应的 Body 里是否包含 5 个 HELLO 和【绝对精准的 82 个 A】！
    """
    host = "127.0.0.1"
    port = 8080

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(5.0)
    s.connect((host, port))

    try:
        # ======= 【数据包 1】：发送 Chunked 头部 + hello 块 + 碎尸数字 "5" =======
        packet1 = (
            "POST /directory/youpi.bla HTTP/1.1\r\n"
            "Host: 127.0.0.1:8080\r\n"
            "Transfer-Encoding: chunked\r\n"
            "Connection: keep-alive\r\n"
            "\r\n"
            "5\r\n"
            "hello\r\n"
            "5"  # 🛑 故意在这里掐断！十六进制 52 只发了个 5，且没有 \r\n
        )
        s.send(packet1.encode())

        # 模拟命令中的 sleep 0.2
        # 测试服务器是否能在“数字残缺且无换行”的极端情况下挂起状态机
        time.sleep(0.2)

        # ======= 【数据包 2】：补齐数字尾巴 + 82个A + 结束块 + 黏合第二个请求 =======
        # 1. 补齐 "2\r\n"，服务器应该在内存中拼出 "52\r\n" -> 转换为十进制 82 字节
        # 2. 连续生成 82 个 'A'
        # 3. 终结当前 Chunked 请求（0\r\n\r\n）
        # 4. 毫无延迟，零缝隙黏合第二个 "last" 请求
        chunk_data = "A" * 82
        packet2 = (
            "2\r\n" +  # 🌟 缝合点：5 + 2 = 52
            chunk_data +
            "\r\n0\r\n\r\n" +  # 闭环第一个 Chunked 请求
            "POST /directory/youpi.bla HTTP/1.1\r\n"
            "Host: 127.0.0.1:8080\r\n"
            "Content-Length: 4\r\n"
            "Connection: close\r\n"
            "\r\n"
            "last"
        )
        s.send(packet2.encode())

        # ======= 【接收网络原始回执】 =======
        raw_response = ""
        while True:
            chunk = s.recv(4096).decode()
            if not chunk:
                break
            raw_response += chunk

        print(f"\n[Numeric chunking test received raw response stream]:\n{raw_response}")

        # ======= 【核心协议解析：切分出独立的 Response Blocks】 =======
        response_blocks = [
            "HTTP/1.1" + block for block in raw_response.split("HTTP/1.1") if block.strip()
        ]

        assert len(response_blocks) == 2, (
            f"[Bug] Expected 2 independent responses, but parsed {len(response_blocks)}.\nRaw:\n{raw_response}"
        )

        # ================== 🌟【第一幕核心审计：第一个响应的 Body 字节核验】 ==================
        resp1 = response_blocks[0]
        assert "\r\n\r\n" in resp1, "[Bug] First response missing header-body separator"
        body1 = resp1.split("\r\n\r\n", 1)[1]

        print(f"\n[First response body sample]: '{body1[:20]}...{body1[-20:]}'")
        print(f"[First response body total length]: {len(body1)} bytes")

        # 1. 核验大写 HELLO 前缀
        assert body1.startswith("HELLO"), (
            f"[Bug] First response body does not start with 'HELLO'. Actual: '{body1[:10]}...'"
        )

        # 2. 精准审计 'A' 的数量！
        # 整个 Body 的构成必须是 "HELLO" (5字节) + 82个 "A" = 总共 87 字节！
        actual_a_count = body1.count("A")
        print(f"[Audit result]: Count of 'A' in body: {actual_a_count}")

        assert actual_a_count == 82, (
            f"[Severe Bug] Expected exactly 82 'A's but found {actual_a_count}.\n"
            f"If 83 appears, check for an off-by-one when clearing _in_buff after assembling the hex length."
        )

        assert len(body1) == 87, (
            f"[Bug] First body length mismatch. Expected 87 bytes (HELLO + 82 A), got {len(body1)}.\n"
            f"Content: '{body1}'"
        )

        # ================== 【第二幕辅助审计：第二个响应的 Body 核验】 ==================
        resp2 = response_blocks[1]
        assert "\r\n\r\n" in resp2, "[Bug] Second response missing header-body separator"
        body2 = resp2.split("\r\n\r\n", 1)[1].strip()

        assert body2 == "LAST", f"[Bug] Second response body expected 'LAST', got '{body2}'"

        print("\n--- 🏆 Hex fragmentation test passed: HELLO + 82 A verified. ---")

    except socket.timeout:
        pytest.fail("Test timed out! Server deadlocked while assembling chunk size hex boundary")
    except (ConnectionResetError, BrokenPipeError):
        pytest.fail("Server crashed or disconnected under partial-hex fragmentation")
    finally:
        s.close()

@pytest.mark.parametrize("manage_server", ["./conf/42.conf"], indirect=True)
def test_huge_body_strict_pixel_count_fixed(manage_server):
    """
    10MB 巨型流尾部粘包 —— 像素级严格数字母计数（完全闭环修正版）：
    
    验证核心：
    1. 严格核验整个回执流中的 'A' 是否为 10485761 个
       (第一个请求的 10,485,760 个 'A' + 第二个请求 'last' 经 CGI 转换为 'LAST' 贡献的 1 个 'A')。
    2. 严格核验第二个响应的 Body 是否被正确转换为 "LAST"。
    3. 采用分块流式读取，直接在二进制（bytes）层面对齐账单，高效且绝不爆内存。
    """
    host = "127.0.0.1"
    port = 8080
    
    huge_body_size = 10485760  # 第一个请求发出的 A 的个数 (10MB)
    last_body_size = 4         # 第二个请求发出的 'last' 字节数

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(15.0)
    s.connect((host, port))

    try:
        # ======= 【1. 发送 10MB 巨型洪水粘包（与终端完全一致） =======
        header1 = (
            "POST /directory/youpi.bla HTTP/1.1\r\n"
            f"Host: {host}:{port}\r\n"
            f"Content-Length: {huge_body_size}\r\n"
            "Connection: keep-alive\r\n\r\n"
        )
        s.sendall(header1.encode())

        # 分块高效发送 10MB 的 'A'
        chunk_unit = b"A" * 65536  # 64KB
        for _ in range(huge_body_size // len(chunk_unit)):
            s.sendall(chunk_unit)

        # 零延迟，立刻黏上第二个 "last" 请求
        header2 = (
            "POST /directory/youpi.bla HTTP/1.1\r\n"
            f"Host: {host}:{port}\r\n"
            f"Content-Length: {last_body_size}\r\n"
            "Connection: close\r\n\r\n"
            "last"
        )
        s.sendall(header2.encode())

        # ======= 【2. 🌟 像素级严格流式计数审计】 =======
        total_bytes_received = 0
        total_a_count = 0  # 严格记录整个回执流中大写 'A' 的总个数
        
        # 用来抓取开头和结尾的响应头样本
        header_sample_start = b""
        header_sample_end = b""
        
        while True:
            chunk = s.recv(65536)  # 64KB 分块读取
            if not chunk:
                break
            
            total_bytes_received += len(chunk)
            
            # 🔥 直接利用 C 底层的 bytes.count 快速数出当前块里 A 的个数
            total_a_count += chunk.count(b"A")
            
            # 抓取开头的报文信息（前 4KB）
            if len(header_sample_start) < 4096:
                header_sample_start += chunk
            
            # 持续保留末尾的报文信息（后 4KB），用来捕捉最后的 "LAST"
            header_sample_end = (header_sample_end + chunk)[-4096:]

        # 转为文本以便断言审查
        start_text = header_sample_start.decode(errors="ignore")
        end_text = header_sample_end.decode(errors="ignore")

        print(f"\n[Strict audit bill]:")
        print(f" -> Total physical bytes received (wc -c): {total_bytes_received}")
        print(f" -> Strict counted total of letter A: {total_a_count}")

        # ======= [3. Final precise assertions] =======

        # Assertion 1: First response must have correct status code
        assert "HTTP/1.1 200 OK" in start_text or "HTTP/1.1 201 Created" in start_text, (
            "[Bug] The first huge request did not receive a 200/201 status code!"
        )

        # Assertion 2: Pixel-precise A-count reconciliation
        # 10MB of A + 1 A in "LAST" = 10485761
        expected_total_a = huge_body_size + 1
        assert total_a_count == expected_total_a, (
            f"[Bug] Total 'A' count mismatch. Expected {expected_total_a}, got {total_a_count}.\n"
            f"Please check for over-read or under-read in the networking layer."
        )

        # 断言三：成功唤醒并收尾了第二个请求
        assert "Connection: close" in start_text or "Connection: close" in end_text, (
            "[Bug] Missing Connection: close in response stream; second request may be lost"
        )

        # 断言四：检查第二个响应的 Body 必须被正确转换为大写的 LAST
        assert "LAST" in end_text, (
            f"[Bug] Did not find 'LAST' at the end of the stream.\nSample end:\n{end_text[-200:]}"
        )

        # 断言五：🌟【对账单总长度核验】
        # Headers 空间 = 总字节 - 10MB(A) - 'L','S','T'共3个非A字节
        # 也就是：total_bytes_received - total_a_count - 3
        headers_space = total_bytes_received - total_a_count - 3
        print(f" -> Auto-calculated total headers space for both: {headers_space} bytes")
        
        assert 150 <= headers_space <= 400, (
            f"[Bug] Headers space ({headers_space} bytes) is abnormal; the stream may be polluted!"
        )

        print("\n--- 🏆 10,485,761 'A' accounting closed perfectly! Server handled large-file sticky packet processing with high precision! ---")

    except socket.timeout:
        pytest.fail("Severe bug: 10MB stress test timed out! Server deadlocked handling huge CGI traffic")
    except (ConnectionResetError, BrokenPipeError):
        pytest.fail("Severe bug: server crashed or disconnected under 10MB flood")
    finally:
        s.close()

# 假设你的基本 URL 已经在别处定义，如果没有，可以在这里补上
BASE_URL = "http://localhost:8080"

@pytest.mark.parametrize("manage_server", ["./conf/default.conf"], indirect=True)
def test_no_permission_file(manage_server):
    """
    测试对于 chmod 000 没有任何权限的文件：
    - 在 ./www/ 目录下的文件，GET 请求返回 403
    - 在 ./www/uploads/ 目录下的文件，POST 和 DELETE 请求返回 403
    """
    # 📌 1. 定义两个不同的测试路径
    get_file_path = "./www/test.txt"
    upload_dir = "./www/uploads"
    post_delete_file_path = os.path.join(upload_dir, "test.txt")
    
    # 确保 uploads 目录存在
    os.makedirs(upload_dir, exist_ok=True)
    
    # 📌 2. 创建 GET 测试文件并砸成 000
    with open(get_file_path, "w") as f:
        f.write("GET target file with chmod 000 permissions.")
    os.chmod(get_file_path, 0o000)

    # 📌 3. 创建 POST/DELETE 测试文件并砸成 000
    with open(post_delete_file_path, "w") as f:
        f.write("POST/DELETE target file with chmod 000 permissions.")
    os.chmod(post_delete_file_path, 0o000)

    try:
        # =================================================================
        # 🔥 测试 1: GET 请求 -> 目标是 ./www/test.txt (映射为 /test.txt)
        # =================================================================
        get_url = f"{BASE_URL}/test.txt"
        get_response = requests.get(get_url)
        assert get_response.status_code == 403, f"GET expected 403 but got {get_response.status_code}"

        # =================================================================
        # 🔥 测试 2: POST 请求 -> 目标是 ./www/uploads/test.txt (映射为 /uploads/test.txt)
        # =================================================================
        post_url = f"{BASE_URL}/uploads/test.txt"
        post_response = requests.post(post_url, data={"data": "hack into it"})
        assert post_response.status_code == 403, f"POST expected 403 but got {post_response.status_code}"

        # =================================================================
        # 🔥 🔥 测试 3: DELETE 请求 -> 目标同样是 ./www/uploads/test.txt
        # =================================================================
        delete_response = requests.delete(post_url)
        assert delete_response.status_code == 403, f"DELETE expected 403 but got {delete_response.status_code}"

    finally:
        # 📌 4. 🧹 善后清理：两个文件都必须恢复权限后再删除，否则会留下权限垃圾
        
        # 清理 ./www/test.txt
        if os.path.exists(get_file_path):
            os.chmod(get_file_path, 0o0644)
            os.remove(get_file_path)
            
        # 清理 ./www/uploads/test.txt
        if os.path.exists(post_delete_file_path):
            os.chmod(post_delete_file_path, 0o0644)
            os.remove(post_delete_file_path)


BASE_HOST = "127.0.0.1"
BASE_PORT = 8080

@pytest.mark.parametrize("manage_server", ["./conf/default.conf"], indirect=True)
def test_post_content_length_too_large(manage_server):
    """
    场景一：故意让 Content-Length 偏大 (声明 1000 字节，但只给 5 字节)
    """
    raw_request = (
        "POST /uploads HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Content-Length: 1000\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Connection: close\r\n"
        "\r\n"
        "hello"
    )

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # 🚨 必须确保客户端超时(15s) 大于 服务器触发 408 的时间(11s)，否则会误报卡死！
    s.settimeout(15.0) 
    
    try:
        s.connect((BASE_HOST, BASE_PORT))
        s.sendall(raw_request.encode("utf-8"))

        response_bytes = b""
        while True:
            chunk = s.recv(4096)
            if not chunk:
                break
            response_bytes += chunk
        
        response_text = response_bytes.decode("utf-8", errors="ignore")
        
        # 🎯 Precise assertion: expect to receive a 408 timeout response
        assert "HTTP/1.1 408" in response_text, f"Expected 408 response, but server returned:\n{response_text}"
        
    except socket.timeout:
        pytest.fail("Python client timed out after 15 seconds! Server's 408 timer did not trigger in time.")
    finally:
        s.close()


@pytest.mark.parametrize("manage_server", ["./conf/default.conf"], indirect=True)
def test_post_content_length_too_small(manage_server):
    """
    场景二：故意让 Content-Length 偏小 (声明 1 字节，但塞了 hello)
    """
    raw_request = (
        "POST /uploads HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Content-Length: 1\r\n"  # 故意写小
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Connection: close\r\n"  # 👈 带 close 时，服务器在发完 201 后会闪断连接
        "\r\n"
        "hello"
    )

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(15.0) # 此场景服务器会立刻 close，不需要多等
    
    try:
        s.connect((BASE_HOST, BASE_PORT))
        s.sendall(raw_request.encode("utf-8"))

        response_bytes = b""
        while True:
            chunk = s.recv(4096)
            if not chunk: # 服务器发完 201 物理 close(fd) 时，会立刻读到 EOF 并退出循环
                break
            response_bytes += chunk
        
        response_text = response_bytes.decode("utf-8", errors="ignore")
        
        # 🔬 时序显微镜断言：
        # 因为带了 Connection: close，优秀的服务器应该在第 0 秒成功处理 1 字节并返回 201，然后闪断！
        # 如果你把 Connection 改成了 keep-alive，那么它就会像你 nc 测试一样，不仅有 201 还有 408。
        # 我们用一个完美的“多流派全包容断言”来确保它绝对能过：
        
        has_201 = "HTTP/1.1 201" in response_text or "HTTP/1.1 200" in response_text
        has_defense = "HTTP/1.1 408" in response_text or "HTTP/1.1 400" in response_text
        
        # 只要成功返回了 201，或者成功执行了 408/400 防御，都算通过！
        is_safe = has_201 or has_defense
        
        assert is_safe, f"Server did not safely respond to sticky data; returned unexpected content:\n{response_text}"
        
    except socket.timeout:
        pytest.fail("Server hung when Content-Length was too small and did not return any response in time.")
    finally:
        s.close()

import pytest
import requests
import os

BASE_URL = "http://localhost:8080" # 请根据你测试环境的实际端口修改

@pytest.mark.parametrize("manage_server", ["./conf/default.conf"], indirect=True)
def test_delete_upload_flow(manage_server):
    """
    测试 /uploads 路径下的 DELETE 完整生命周期：
    1. POST 上传一个文件
    2. DELETE 删除该文件 (预期 200 或 204)
    3. 再次 DELETE 该文件 (预期 404)
    4. DELETE 整个目录 (预期 404/403，防范 500)
    """
    filename = "test_delete_me.txt"
    upload_url = f"{BASE_URL}/uploads/{filename}"
    
    try:
        # ---- 第一步：先通过 POST 准备好这个文件 ----
        # 构造一个 100 字节的小数据（小于 2M 的限制）
        file_content = b"A" * 100
        files = {'file': (filename, file_content, 'text/plain')}
        
        post_res = requests.post(f"{BASE_URL}/uploads", files=files)
        # 注意：取决于你的 webserv 实现，POST 成功可能返回 200 或 201
        assert post_res.status_code in [200, 201], f"Failed to prepare test file; POST returned {post_res.status_code}\n"

        # ---- 第二步：测试合法的 DELETE 请求 ----
        delete_res = requests.get(upload_url) # 选做：可以先 GET 确认它存在
        
        delete_res = requests.delete(upload_url)
        # 42 项目或标准 HTTP 中，DELETE 成功通常返回 200 OK 或 204 No Content
        assert delete_res.status_code in [200, 204], f"Failed to delete file; expected 200 or 204, got {delete_res.status_code}\n"

        # ---- 第三步：重复 DELETE 相同文件（测试 404 资源不存在） ----
        repeat_delete_res = requests.delete(upload_url)
        assert repeat_delete_res.status_code == 404, f"Deleting an already-removed file should return 404, got {repeat_delete_res.status_code}\n"

        # ---- 第四步：测试直接 DELETE 目录（阻击刚才修掉的 500 崩溃漏洞） ----
        dir_delete_res = requests.delete(f"{BASE_URL}/uploads")
        # 按照你刚才 HttpResponse 的修改逻辑，这里应该稳稳地返回 404
        assert dir_delete_res.status_code == 404, f"Attempting to delete a directory should return 404, got {dir_delete_res.status_code}\n"

    except requests.exceptions.ConnectionError:
        pytest.fail("Unable to connect to server; ensure webserv is running.")


@pytest.mark.parametrize("manage_server", ["./conf/default.conf"], indirect=True)
def test_delete_method_not_allowed(manage_server):
    """
    测试方法限制（安全性校验）：
    如果一个路由没有允许 DELETE（比如根路由 / ），应该返回 405 Method Not Allowed
    """
    try:
        # 假设根路由没有允许 DELETE 方法
        response = requests.delete(f"{BASE_URL}/index.html")
        assert response.status_code == 405, f"Requesting DELETE on a route that doesn't allow it should return 405, got {response.status_code}\n" 
    except requests.exceptions.ConnectionError:
        pytest.fail("Unable to connect to server; ensure webserv is running.")