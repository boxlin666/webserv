import { describe, test, expect } from 'vitest';
import { TestClient } from './TestClient.js';

const client = new TestClient(8080);

describe('并发测试', () => {
    test('50 个并发 GET / 全部应返回 200', async () => {
        const tasks = Array.from({ length: 50 }, () => client.get('/'));
        const results = await Promise.allSettled(tasks);

        const fulfilled = results.filter(r => r.status === 'fulfilled');
        const rejected  = results.filter(r => r.status === 'rejected');

        console.log(`成功: ${fulfilled.length}, 失败: ${rejected.length}`);
        if (rejected.length > 0) {
            rejected.forEach(r => console.log('失败原因:', (r as PromiseRejectedResult).reason));
        }

        // 所有请求必须完成（不能有连接错误）
        expect(rejected.length).toBe(0);
        // 所有响应必须是 200
        fulfilled.forEach(r => {
            expect((r as PromiseFulfilledResult<{ status: number }>).value.status).toBe(200);
        });
    }, 15000);

    test('50 个并发 GET / 响应时间应在合理范围内', async () => {
        const start = Date.now();
        const tasks = Array.from({ length: 50 }, () => client.get('/'));
        await Promise.all(tasks);
        const elapsed = Date.now() - start;

        console.log(`50 并发耗时: ${elapsed}ms`);
        // 50 个并发不应该比串行慢太多，设 10s 上限
        expect(elapsed).toBeLessThan(10000);
    }, 15000);

    test('混合并发：GET 和 POST 同时发送', async () => {
        const gets  = Array.from({ length: 25 }, () => client.get('/'));
        const posts = Array.from({ length: 25 }, () =>
            client.post('/uploads', 'concurrent=true', { 'Content-Type': 'text/plain' })
        );

        const results = await Promise.allSettled([...gets, ...posts]);
        const rejected = results.filter(r => r.status === 'rejected');

        console.log(`混合并发失败数: ${rejected.length}`);
        expect(rejected.length).toBe(0);
    }, 15000);
});