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

def get_form_data():
    """纯手写解析 C++ 喂进标准输入的 Request Body"""
    try:
        content_length = int(os.environ.get('CONTENT_LENGTH', 0))
        if content_length == 0:
            return {}
        raw_body = sys.stdin.read(content_length)
        parsed_dict = urllib.parse.parse_qs(raw_body)
        return {k: v[0] for k, v in parsed_dict.items()}
    except Exception:
        return {}

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

user_id = get_user_id_from_cookie()
# 🌟 数据库文件夹也锁定在当前脚本目录下的 database 内
db_dir = os.path.join(SCRIPT_DIR, "database")

# ----------------- 分流 1：🆕 收到注销请求 -----------------
if is_logout_request():
    # 如果用户带了钥匙过来，我们去后方大本营把档案袋【物理毁灭】
    if user_id:
        session_file = os.path.join(db_dir, f"{user_id}.txt")
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
elif user_id and os.path.exists(os.path.join(db_dir, f"{user_id}.txt")):
    with open(os.path.join(db_dir, f"{user_id}.txt"), "r") as f:
        lines = f.readlines()
    username = lines[0].strip()
    password = lines[1].strip()
    
    print("\r")
    success_page_path = os.path.join(SCRIPT_DIR, "success.html")
    with open(success_page_path, "r", encoding="utf-8") as page:
        print(page.read().replace("{{USERNAME}}", username).replace("{{PASSWORD}}", password))

# ----------------- 分流 3：提交表单登录 或 第一次空手来 -----------------
else:
    form = get_form_data()
    name = form.get("username")
    pwd = form.get("password")
    
    if name and pwd: # 用户填写了账号密码提交
        new_id = str(uuid.uuid4())
        expires = time.strftime("%a, %d-%b-%Y %H:%M:%S GMT", time.gmtime(time.time() + 3600))
        
        print(f"Set-Cookie: id={new_id}; Expires={expires}; Path=/\r")
        print("\r")
        
        if not os.path.exists(db_dir):
            os.makedirs(db_dir)
        with open(os.path.join(db_dir, f"{new_id}.txt"), "w") as f:
            f.write(f"{name}\n{pwd}")
            
        success_page_path = os.path.join(SCRIPT_DIR, "success.html")
        with open(success_page_path, "r", encoding="utf-8") as page:
            print(page.read().replace("{{USERNAME}}", name).replace("{{PASSWORD}}", pwd))
            
    else: # 既没带 Cookie，也没填表单，空手第一次来
        print("\r")
        login_page_path = os.path.join(SCRIPT_DIR, "login_index.html")
        with open(login_page_path, "r", encoding="utf-8") as page:
            print(page.read())