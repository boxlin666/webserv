import { describe, test, expect } from 'vitest';
import { TestClient } from './TestClient.js';

const client = new TestClient(8080);

describe('POST body 解析', () => {
    test('POST 表单数据到 /uploads 应返回 201', async () => {
        const res = await client.post('/uploads', 'name=42student', {
            'Content-Type': 'application/x-www-form-urlencoded',
        });
        expect(res.status).toBe(201);
    });

    test('POST 携带自定义请求头应返回 201', async () => {
        const res = await client.post('/uploads', 'data', {
            'Content-Type': 'text/plain',
            'X-Custom': 'test',
        });
        expect(res.status).toBe(201);
    });

    test('POST 到不允许的路径应返回 405', async () => {
        // / 只允许 GET HEAD，POST 应该被拒绝
        const res = await client.post('/', 'name=42student');
        expect(res.status).toBe(405);
    });
});