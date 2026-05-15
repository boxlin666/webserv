import pytest
import subprocess
import time
import os
import socket
import shutil

BASE_DIR = os.path.dirname(os.path.abspath(__file__)) 
PROJECT_ROOT = os.path.abspath(os.path.join(BASE_DIR, ".."))

def setup_test_env(project_root):
    """
    自动化创建测试环境，并设置严格的 Unix 权限
    """
    youpi_path = os.path.join(project_root, "YoupiBanane")
    
    # 1. 目录权限：0o755 (drwxr-xr-x) 确保服务器有权进入目录
    os.makedirs(os.path.join(youpi_path, "nop"), mode=0o755, exist_ok=True)
    os.makedirs(os.path.join(youpi_path, "Yeah"), mode=0o755, exist_ok=True)
    # 显式确保父目录也是 755
    os.chmod(youpi_path, 0o755)

    # 2. 定义文件及其对应的权限要求
    # 0o644: 普通读取 (rw-r--r--)
    # 0o755: 可执行 (rwxr-xr-x) -> 针对 .bla 或 CGI 相关文件
    test_files = {
        "youpi.bad_extension": 0o777,
        "youpi.bla": 0o777,  # 假设这是 CGI 脚本
        "nop/youpi.bad_extension": 0o777,
        "nop/other.pouic": 0o777,
        "Yeah/not_happy.bad_extension": 0o777
    }

    for file_rel_path, mode in test_files.items():
        full_path = os.path.join(youpi_path, file_rel_path)
        # 创建文件
        if not os.path.exists(full_path):
            with open(full_path, 'a'):
                os.utime(full_path, None)
        # 显式设置权限（关键：不受本地 umask 影响）
        os.chmod(full_path, mode)

@pytest.fixture
def manage_server(request):
    config_rel_path = getattr(request, "param", "./conf/default.conf")
    server_path = os.path.join(PROJECT_ROOT, "webserv")
    config_path = os.path.join(PROJECT_ROOT, config_rel_path)

    # --- 启动前准备 ---
    # 确保 webserv 程序本身有执行权限 (GitHub Actions 必备)
    if os.path.exists(server_path):
        os.chmod(server_path, 0o777)
    
    setup_test_env(PROJECT_ROOT)

    # --- 启动进程 ---
    print(f"\n[Setup] Starting webserv with: {config_rel_path}")
    process = subprocess.Popen(
        [server_path, config_path], 
        cwd=PROJECT_ROOT,
        stdout=None, 
        stderr=None
    )

    # --- 轮询端口 ---
    timeout = 5
    start_time = time.time()
    opened = False
    while time.time() - start_time < timeout:
        if process.poll() is not None: # 进程意外退出了
            break
            
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            # 注意：如果你的配置监听了不同端口，这里需要从 config 动态获取
            if s.connect_ex(('127.0.0.1', 8080)) == 0:
                opened = True
                break
        time.sleep(0.5)

    yield {"process": process, "opened": opened}
    
    # --- 清理 ---
    print(f"\n[Teardown] Stopping server ({config_rel_path})...")
    process.terminate()
    try:
        process.wait(timeout=2)
    except subprocess.TimeoutExpired:
        process.kill()