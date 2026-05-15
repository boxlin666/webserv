import pytest
import subprocess
import time
import os
import socket
import urllib.request
import shutil

BASE_DIR = os.path.dirname(os.path.abspath(__file__)) # test 目录
PROJECT_ROOT = os.path.abspath(os.path.join(BASE_DIR, "..")) # 项目根目录

@pytest.fixture
def manage_server(request):
    """
    通过 request.param 接收配置文件路径
    如果不传参数，默认使用 './conf/default.conf'
    """
    # 1. 获取配置路径（如果调用时没给参数，使用默认值）
    config_rel_path = getattr(request, "param", "./conf/default.conf")
    
    current_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.abspath(os.path.join(current_dir, ".."))
    server_path = os.path.join(project_root, "webserv")
    config_path = os.path.join(project_root, config_rel_path)

    # --- 自动化创建测试环境 (保持不变) ---
    setup_test_env(project_root) # 建议把那堆 mkdir 封装成函数

    # 2. 启动进程
    print(f"\n[Setup] Starting webserv with: {config_rel_path}")
    process = subprocess.Popen(
        [server_path, config_path], 
        cwd=project_root,
        stdout=None, 
        stderr=None
    )

    # 3. 轮询端口确认是否启动
    timeout = 5
    start_time = time.time()
    opened = False
    while time.time() - start_time < timeout:
        # 检查进程是否还没启动就挂了（针对 KO 测试非常重要）
        if process.poll() is not None:
            break
            
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            if s.connect_ex(('127.0.0.1', 8080)) == 0:
                opened = True
                break
        time.sleep(0.5)

    # 注意：如果是为了测试“错误的配置”，我们可能预期 opened 为 False
    # 这里我们把 process 返回给测试用例，让用例自己判断是否启动成功
    yield {"process": process, "opened": opened}
    
    # --- 4. [Teardown] ---
    print(f"\n[Teardown] Stopping server ({config_rel_path})...")
    process.terminate()
    try:
        process.wait(timeout=2)
    except subprocess.TimeoutExpired:
        process.kill()

def setup_test_env(project_root):
    youpi_path = os.path.join(project_root, "YoupiBanane")
    
    # 仅在目录不存在时创建，不使用 rmtree
    os.makedirs(os.path.join(youpi_path, "nop"), exist_ok=True)
    os.makedirs(os.path.join(youpi_path, "Yeah"), exist_ok=True)
    
    # 使用 'a' (append) 模式创建文件，如果已存在则不会覆盖内容，只会更新时间戳
    paths = [
        "youpi.bad_extension", "youpi.bla", 
        "nop/youpi.bad_extension", "nop/other.pouic",
        "Yeah/not_happy.bad_extension"
    ]
    for p in paths:
        full_path = os.path.join(youpi_path, p)
        if not os.path.exists(full_path):
            open(full_path, 'a').close()