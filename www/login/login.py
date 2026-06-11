#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import uuid
import time
import urllib.parse

# 先强行告诉浏览器，我们要吐出的都是标准的 UTF-8 HTML 网页
print("Content-Type: text/html; charset=utf-8\r")

# 🌟 核心安全升级：动态获取当前 login.py 脚本所在的绝对目录路径
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

def parse_request_body():
    """parsing request body (query string) input ex: username=cat&color=orange"""
    """output ex: {"username: ["cat"], "color": ["orange"]"} """
    try:
        content_length = int(os.environ.get('CONTENT_LENGTH', 0))
        if content_length == 0:
            return {}
        raw_body = sys.stdin.read(content_length)

        """ urllib.parse.parse_qs converts "username=cat" into {"username": ["cat"]}
         In HTTP, one key can have multiple values, so the value is always an Array.
         Example: {"username": ["cat", "dog"]} -> Index 0 is "cat", Index 1 is "dog"."""
        parsed_dict = urllib.parse.parse_qs(raw_body)

        """ clean_form is equivalent to std::map<std::string, std::string> in cpp"""
        clean_form = {}

        """iterate the hash map: key points to the value Array"""
        for key, value_array in parsed_dict.items():
            """Key points to the Array. We extract the 1st string at Index 0"""
            first_value = value_array[0]
            clean_form[key] = first_value 
        return clean_form

    except Exception:
        return {}

# ================== 主程序逻辑分流 ==================

#user_id = get_user_id_from_cookie()
# 🌟 数据库文件夹也锁定在当前脚本目录下的 database 内
database_dir = os.path.join(SCRIPT_DIR, "database")

form = parse_request_body()
name = form.get("username")
pwd = form.get("password")

# ----------------- Case 1: form submission or 1st Visit -----------------
"""curl -v -X POST -d "username=cat&password=123" http://localhost:8080/login/login.py"""
"""curl -v -X POST -d "username=cat&password=123" http://localhost:8080/login/ (complete the cgi script filename inside the webserv program)"""
"""curl -v GET http://localhost:8080/login/login.py"""
"""curl -v GET http://localhost:8080/login/ (complete the cgi script filename inside the webserv program)"""
if name and pwd: #form submission
    new_id = str(uuid.uuid4()) #Unique Universal Identifier generated (Session ID)
    expires = time.strftime("%a, %d-%b-%Y %H:%M:%S GMT", time.gmtime(time.time() + 3600)) #Session ID expired in 1h
    
    #Set-Cooke reponse header triggers the browser to create a local cookie file (client side)
    print(f"Set-Cookie: id={new_id}; Expires={expires}; Path=/\r") 
    print("\r")

    #Create database folder if not exist
    if not os.path.exists(database_dir): 
        os.makedirs(database_dir)
    #Create a new file in this database folder, and name it with the session id, fill it out with username + password!
    with open(os.path.join(database_dir, f"{new_id}.txt"), "w") as f:
        f.write(f"{name}\n{pwd}")

    #Extract the success.html template and replace usernane and password of this page
    success_page_path = os.path.join(SCRIPT_DIR, "success.html")
    with open(success_page_path, "r", encoding="utf-8") as page:
        print(page.read().replace("{{USERNAME}}", name).replace("{{PASSWORD}}", pwd))
else: # 1st visit
    print("\r")
    login_page_path = os.path.join(SCRIPT_DIR, "login_index.html")
    with open(login_page_path, "r", encoding="utf-8") as page:
        print(page.read())

def get_user_id_from_cookie():
    """查找过程：从 C++ 的环境变量中抓取旧的 Cookie ID"""
    cookies = os.environ.get('HTTP_COOKIE', '')
    if cookies:
        for cookie in cookies.split(';'):
            parts = cookie.strip().split('=', 1)
            if len(parts) == 2 and parts[0] == "id":
                return parts[1]
    return None

def is_logout_request():
    """🆕 检查当前请求是否带有 ?action=logout 的注销信号"""
    query_string = os.environ.get('QUERY_STRING', '')
    params = urllib.parse.parse_qs(query_string)
    return "logout" in params.get("action", [])

# ================== 主程序逻辑分流 ==================
"""
# ----------------- 分流 1：🆕 收到注销请求 -----------------
if is_logout_request():
    # 如果用户带了钥匙过来，我们去后方大本营把档案袋【物理毁灭】
    if user_id:
        session_file = os.path.join(database_dir, f"{user_id}.txt")
        if os.path.exists(session_file):
            try:
                os.remove(session_file) # 斩草除根！
            except Exception:
                pass

    # 核心：下发毒药 Cookie（Max-Age=0），命令客户端本地硬盘立刻抹除 id
    print("Set-Cookie: id=deleted; Max-Age=0; Path=/\r")
    print("\r")

    # 注销完毕后，直接重新加载干净的登录页面，强行重置状态
    login_page_path = os.path.join(SCRIPT_DIR, "login_index.html")
    with open(login_page_path, "r", encoding="utf-8") as page:
        print(page.read())

# ----------------- 分流 2：正常带 Cookie 登录成功 -----------------
elif user_id and os.path.exists(os.path.join(database_dir, f"{user_id}.txt")):
    with open(os.path.join(database_dir, f"{user_id}.txt"), "r") as f:
        lines = f.readlines()
    username = lines[0].strip()
    password = lines[1].strip()
    
    print("\r")
    success_page_path = os.path.join(SCRIPT_DIR, "success.html")
    with open(success_page_path, "r", encoding="utf-8") as page:
        print(page.read().replace("{{USERNAME}}", username).replace("{{PASSWORD}}", password))"""