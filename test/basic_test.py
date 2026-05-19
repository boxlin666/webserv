import requests
import pytest
import socket
import time
import os

# 配置你的服务器地址
BASE_URL = "http://localhost:8080"

#========================= Configuration file default.conf=========================================
@pytest.mark.parametrize("manage_server", ["./conf/default.conf"], indirect=True)
def test_get_index(manage_server):
    """
    测试基本的 GET 请求是否能正常获取主页
    """ 
    try:
        response = requests.get(f"{BASE_URL}/index.html")
        assert response.status_code == 200
        # 检查响应头是否符合 42 项目要求的规范
        assert "Content-Length" in response.headers
    except requests.exceptons.ConnectionError:
        pytest.fail("无法连接到服务器，请确保 webserv 已启动。") 

@pytest.mark.parametrize("manage_server", ["./conf/default.conf"], indirect=True)
def test_404_not_found(manage_server):
    """
    测试请求不存在的文件时，服务器是否返回 404
    """
    response = requests.get(f"{BASE_URL}/non_existent_file.html")
    assert response.status_code == 404

@pytest.mark.parametrize("manage_server", ["./conf/default.conf"], indirect=True)
def test_method_not_allowed(manage_server):
    """
    测试对静态文件使用不允许的 POST 方法（假设你的配置限制了方法）
    """
    # 假设 /index.html 只允许 GET
    response = requests.post(f"{BASE_URL}/test/index.html", data={"key": "value"})
    # 根据你的配置，可能是 405 Method Not Allowed
    assert response.status_code == 405

SERVER_WEB_ROOT = "./www"

def get_physical_path(url_path):
    """
    辅助函数：将 URL 路径转换为本地磁盘的物理路径
    例如: f"{BASE_URL}/uploads/file.txt" -> "./www/uploads/file.txt"
    """
    relative_path = url_path.replace(BASE_URL, "").lstrip("/")
    return os.path.join(SERVER_WEB_ROOT, relative_path)

@pytest.fixture
def cleanup_files():
    """
    自动化清理神器：收集测试中产生的文件，并在结束后全部扬了
    """
    files_to_clean = []
    # yield 把控制权交还给测试用例
    yield files_to_clean
    
    # 【测试结束后的清理阶段】
    print("\n--- 开始清理测试生成的残留文件 ---")
    for file_path in files_to_clean:
        if os.path.exists(file_path):
            try:
                os.remove(file_path)
                print(f" 成功删除残留文件: {file_path}")
            except Exception as e:
                print(f"❌ 无法删除文件 {file_path}: {e}")


@pytest.mark.parametrize("manage_server", ["./conf/default.conf"], indirect=True)
def test_post_new_static_file(manage_server, cleanup_files):
    """
    测试场景一：新建静态文件 (201 Created) -> 结束后自动删除
    """
    target_url = f"{BASE_URL}/uploads/new_test_file.txt"
    payload = "Hello Webserv 42 project! This is binary raw data post."
    
    # 注册这个文件，等下测试完了它会被自动删除
    cleanup_files.append(get_physical_path(target_url))
    
    try:
        response = requests.post(target_url, data=payload)
        assert response.status_code == 201
        assert "Content-Length" in response.headers
        
    except requests.exceptions.ConnectionError:
        pytest.fail("无法连接到服务器，请确保 webserv 已启动。")


@pytest.mark.parametrize("manage_server", ["./conf/default.conf"], indirect=True)
def test_post_overwrite_static_file(manage_server, cleanup_files):
    """
    测试场景二：覆盖旧文件 (200 OK) -> 结束后自动删除
    """
    target_url = f"{BASE_URL}/uploads/overwrite_test.txt"
    
    # 注册这个文件，完事了一并清除
    cleanup_files.append(get_physical_path(target_url))
    
    try:
        # 1. 第一次创建
        res1 = requests.post(target_url, data="First content")
        assert res1.status_code == 201
        
        # 2. 第二次覆盖
        res2 = requests.post(target_url, data="Second fresh content")
        assert res2.status_code == 200
        
    except requests.exceptions.ConnectionError:
        pytest.fail("无法连接到服务器，请确保 webserv 已启动。")


@pytest.mark.parametrize("manage_server", ["./conf/default.conf"], indirect=True)
def test_post_conflict_with_directory(manage_server):
    """
    测试场景三：往已有目录路径直接写入实体文件 (403 Forbidden)
    由于这个操作会被你的 C++ 代码在门口拦截，【根本不会在磁盘上生成新文件】，所以不需要清理
    """
    target_url = f"{BASE_URL}/uploads/"
    
    try:
        response = requests.post(target_url, data="Dangerous data")
        assert response.status_code == 403
        
    except requests.exceptions.ConnectionError:
        pytest.fail("无法连接到服务器，请确保 webserv 已启动。")

#========================= Configuration file 42.conf=========================================
@pytest.mark.parametrize("manage_server", ["./conf/42.conf"], indirect=True)
def test_fragmented_header(manage_server):
    
    #核心测试：模拟 Header 在传输过程中被切断 (Accept-Encodin + g: gzip)
    #测试服务器的 in_buff 拼接能力。
    
    host = "127.0.0.1"
    port = 8080
    
    # 1. 创建原生 TCP Socket
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(5) # 防止服务器不响应导致死锁
    s.connect((host, port))

    # 2. 发送第一部分：故意在字段名中间切断
    part1 = (
        "GET / HTTP/1.1\r\n"
        "Host: 127.0.0.1:8080\r\n"
        "User-Agent: Go-http-client/1.1\r\n"
        "Accept-Encodin"  # 故意截断
    )
    s.send(part1.encode())
    
    # 3. 关键：停顿一下，确保服务器的 poll() 触发 POLLIN 
    # 且 read() 读到了这部分不完整的数据
    time.sleep(0.5)

    # 4. 发送第二部分：补全剩下的内容并发送结尾的双换行 (\r\n\r\n)
    part2 = "g: gzip\r\n\r\n"
    s.send(part2.encode())

    # 5. 接收响应
    try:
        response = s.recv(4096).decode()
        assert "HTTP/1.1 200 OK" in response
        assert "Content-Length" in response
    except socket.timeout:
        pytest.fail("服务器在接收碎片化 Header 后超时未响应。")
    finally:
        s.close()

@pytest.mark.parametrize("manage_server", ["./conf/42.conf"], indirect=True)
def test_split_crlf_crlf(manage_server):
    
    #进阶测试：模拟 Header 的结束标志 \r\n\r\n 被切断的情况

    host = "127.0.0.1"
    port = 8080
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((host, port))

    # 发送全部 Header 及其最后一个 \r
    s.send(b"GET / HTTP/1.1\r\nHost: localhost\r\n\r")
    time.sleep(0.2)
    # 发送剩下的 \n\r\n
    s.send(b"\n\r\n")

    response = s.recv(4096).decode()
    assert "HTTP/1.1 200 OK" in response
    s.close()

@pytest.mark.parametrize("manage_server", ["./conf/42.conf"], indirect=True)
def test_fragmented_chunked_success(manage_server):
    """
    流式碎片发送测试（成功流）：
    将 HTTP 报文切成极度细碎的片段发送，验证服务器的状态机能否在非阻塞/消除模式下，
    完美拼接出完整的请求，并最终成功返回 200 OK。
    """
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
        assert "200" in response, f"【Bug】服务器未能成功解析分块碎片，回了错误响应：{response}"

    except (socket.timeout):
        pytest.fail("【Bug】数据明明全发完了，但服务器卡在状态机里装死，导致客户端读取超时")
    except (ConnectionResetError, BrokenPipeError):
        pytest.fail("【Bug】数据传输过程中，服务器状态机误判，提前挂断了正常的连接")
    finally:
        s.close()