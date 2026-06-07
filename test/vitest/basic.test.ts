import { test, expect } from 'vitest';
import { TestClient } from './TestClient.js'; // 确保路径正确

test('Minimal Test: Should receive HTTP 200 from webserv', async () => {
    // 实例化 Client
    const client = new TestClient(8080);
    
    // 调用封装好的 get 方法
    // TestClient 内部已经处理了 data 累积和 socket 关闭
    const response = await client.get('/');
    
    // 使用解析后的状态码进行断言 (假设你的 TestClient 解析了 status)
    // 如果你的 TestClient 还没解析 status，可以改为 expect(response.raw).toContain('HTTP/1.1 200');
    expect(response.status).toBe(200);
});