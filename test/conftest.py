import pytest
import subprocess
import time
import os
import socket
import urllib.request
import shutil

BASE_DIR = os.path.dirname(os.path.abspath(__file__)) # test 目录
PROJECT_ROOT = os.path.abspath(os.path.join(BASE_DIR, "..")) # 项目根目录
CGI_TESTER_PATH = os.path.join(PROJECT_ROOT, "cgi_tester") # 目标文件绝对路径

def download_cgi_test(target):
    url = "https://cdn.intra.42.fr/document/document/46605/cgi_tester"
    if not os.path.exists(target):
        print(f"\n[CRITICAL] cgi_tester missing at {target}")
        print(f"[Setup] Downloading from 42 CDN...")
        try:
            # 增加一个请求头，模拟浏览器，防止被服务器拒绝
            req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
            with urllib.request.urlopen(req) as response, open(target, 'wb') as out_file:
                shutil.copyfileobj(response, out_file)
            
            os.chmod(target, 0o755)
            print(f"[Setup] Success! File saved to: {target}")
        except Exception as e:
            pytest.exit(f"\n[FATAL ERROR] Download failed: {e}")
    else:
        print(f"\n[Setup] cgi_tester already exists at {target}")

@pytest.fixture(scope="session", autouse=True)
def manage_server():
    current_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.abspath(os.path.join(current_dir, ".."))
    server_path = os.path.join(project_root, "webserv")
    
    # 假设你的配置文件叫 default.conf，且在根目录下
    config_path = os.path.join(project_root, "./conf/default.conf")

# --- 新增：自动化创建测试环境 ---
    youpi_path = os.path.join(project_root, "YoupiBanane")
    if os.path.exists(youpi_path):
        shutil.rmtree(youpi_path)
    
    os.makedirs(os.path.join(youpi_path, "nop"))
    os.makedirs(os.path.join(youpi_path, "Yeah"))
    
    # 按照学校要求创建文件
    open(os.path.join(youpi_path, "youpi.bad_extension"), 'a').close()
    open(os.path.join(youpi_path, "youpi.bla"), 'a').close()
    open(os.path.join(youpi_path, "nop/youpi.bad_extension"), 'a').close()
    open(os.path.join(youpi_path, "nop/other.pouic"), 'a').close()
    open(os.path.join(youpi_path, "Yeah/not_happy.bad_extension"), 'a').close()

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
    
# --- 2. [Teardown] 清理工作 ---
    print("\n[Teardown] Cleaning up test environment...")
    
    # 停止服务器进程
    process.terminate()
    process.wait()

    # 删除生成的测试文件夹
    #if os.path.exists(youpi_path):
        #shutil.rmtree(youpi_path)
