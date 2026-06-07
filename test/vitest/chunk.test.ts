import { test, expect } from 'vitest';
import { TestClient } from './TestClient.js';

test('Fragmented Request: Should handle chunked data receiving', async () => {
    const client = new TestClient(8080);
    
    // 使用 sendChunks 模拟分片发送
    const res = await client.sendChunks([
        'GET / HTTP/1.1\r\n',
        'Host: localhost\r\nConnection: close\r\n\r\n'
    ], 50); // 间隔 50ms

    expect(res.status).toBe(200);
});