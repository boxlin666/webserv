import { test, expect } from 'vitest';
import net from 'net';

test('Minimal Test: Should receive HTTP 200 from webserv', async () => {
    // 1. 创建一个 Promise 来处理异步响应
    const response = await new Promise<string>((resolve, reject) => {
        const client = net.createConnection({ port: 8080, host: '127.0.0.1' });

        client.on('connect', () => {
            // 发送一个最基础的 HTTP 请求
            client.write('GET / HTTP/1.1\r\nHost: localhost\r\n\r\n');
        });

        client.on('data', (data) => {
            resolve(data.toString());
            client.end();
        });

        client.on('error', (err) => reject(err));
    });

    // 2. 验证响应（检查是否包含 200 OK）
    expect(response).toContain('HTTP/1.1 200 OK');
});

test('Fragmented Request: Should handle chunked data receiving', async () => {
    const result = await new Promise<string>((resolve, reject) => {
        const client = net.createConnection({ port: 8080, host: '127.0.0.1' });
        let responseData = '';

        client.on('connect', () => {
            // 故意拆分成两次发送，测试服务器是否会因为只收到一半 Header 而卡住
            client.write('GET / HTTP/1.1\r\n');
            setTimeout(() => {
                client.write('Host: localhost\r\nConnection: close\r\n\r\n');
            }, 50); // 间隔 50ms 发送剩下的部分
        });

        client.on('data', (chunk) => {
            responseData += chunk.toString();
            // 检查是否已经接收到了完整的 HTTP 响应（简单判断）
            if (responseData.includes('\r\n\r\n')) {
                resolve(responseData);
                client.end(); // 告诉服务器你可以关闭这个连接了
            }
        });

        client.on('end', () => resolve(responseData));
        client.on('error', (err) => reject(err));
    });

    // 验证结果
    expect(result).toContain('HTTP/1.1 200');
});