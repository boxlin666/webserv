import subprocess
import json

def run_siege(concurrency, time):
    # -j 参数强制 siege 输出 JSON 格式，便于代码处理
    # -q 参数关闭详细日志输出，只保留最后的结果
    cmd = f"siege -j -c {concurrency} -t {time} http://localhost:8080/"
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    
    # 查找 JSON 输出的起始位置（防止 siege 输出一些杂项提示）
    output = result.stdout
    start_idx = output.find('{')
    if start_idx == -1:
        return None
    
    return json.loads(output[start_idx:])

def compare_results(current, baseline):
    print(f"--- 测试结果对比 ---")
    # 直接使用 key 访问 JSON 对象
    failed = current.get('failed_transactions', 0)
    trans = current.get('transactions', 0)
    
    if failed > 0:
        print(f"❌ 警告：发现 {failed} 个失败请求！")
    else:
        print("✅ 无失败请求")
        
    if trans < baseline['transactions'] * 0.9:
        print(f"⚠️ 警告：性能下降！本次 {trans}，基准 {baseline['transactions']}")
    else:
        print(f"✅ 性能达标 (当前: {trans})")

# 主逻辑
if __name__ == "__main__":
    baseline = {"transactions": 50000}
    print("开始压力测试...")
    stats = run_siege(10, "10S")
    
    if stats:
        print(f"本次测试结果: {stats}")
        compare_results(stats, baseline)
    else:
        print("解析失败：未获取到有效的 JSON 数据")