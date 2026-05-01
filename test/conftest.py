import pytest
import subprocess
import time
import os
import socket

@pytest.fixture(scope="session", autouse=True)
def manage_server():
    current_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.abspath(os.path.join(current_dir, ".."))
    server_path = os.path.join(project_root, "webserv")
    
    # 假设你的配置文件叫 default.conf，且在根目录下
    config_path = os.path.join(project_root, "./conf/default.conf")

    # 这里的列表就像你在终端输入：./webserv default.conf
    process = subprocess.Popen(
        [server_path, config_path], 
        cwd=project_root,
        stdout=None, # 建议先设为 None，这样报错能直接打在屏幕上
        stderr=None
    )
    # 3. 轮询端口确认是否启动
    timeout = 5 #先开./webserv ./conf/default.conf 程序，延后5s启动pytest 
    start_time = time.time()
    opened = False
    while time.time() - start_time < timeout:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            if s.connect_ex(('127.0.0.1', 8080)) == 0:
                opened = True
                break
        time.sleep(0.5)

    if not opened:
        process.terminate()
        pytest.exit("C++ Webserver 启动超时，检查端口是否正确。")

    yield 
    
    process.terminate()
    process.wait()
