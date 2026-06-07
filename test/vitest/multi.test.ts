import { test, expect, beforeAll } from 'vitest';
import { TestClient } from './TestClient.js';

test('Concurrency: 50 simultaneous clients', async () => {
    const client = new TestClient(8080);
    
    // 启动 50 个并发任务
    const tasks = Array.from({ length: 50 }).map(() => client.get('/'));
    
    // 等待所有任务完成
    const results = await Promise.all(tasks);
    
    // 验证所有请求都成功
    results.forEach(res => {
        expect(res.status).toBe(200);
    });
});