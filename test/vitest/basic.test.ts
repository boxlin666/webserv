import { describe, test, expect, beforeAll } from 'vitest';
import { TestClient } from './TestClient.js';

const client = new TestClient(8080);

describe('GET 请求', () => {
    test('GET / 应返回 200', async () => {
        const res = await client.get('/');
        expect(res.status).toBe(200);
    });

    test('GET 不存在的路径应返回 404', async () => {
        const res = await client.get('/this-does-not-exist');
        expect(res.status).toBe(404);
    });

    test('响应头包含 Content-Type', async () => {
        const res = await client.get('/');
        expect(res.headers).toHaveProperty('content-type');
    });

    test('响应 body 非空', async () => {
        const res = await client.get('/');
        expect(res.body.length).toBeGreaterThan(0);
    });
});

describe('POST 请求', () => {
    test('POST / 应返回合法状态码', async () => {
        const res = await client.post('/', 'hello=world', {
            'Content-Type': 'application/x-www-form-urlencoded',
        });
        expect([200, 201, 204, 405]).toContain(res.status);
    });

    test('Content-Length 不匹配时服务器应返回 400', async () => {
        
        const res = await client.request('POST', '/', 'hello', {
            'Content-Type': 'text/plain',
            'Content-Length': '999',
        });
        expect(res.status).toBe(400);
    }, 1200); // 明确超时时间，比默认的 5s 更快失败
});

describe('分块发送（慢客户端模拟）', () => {
    test('请求头分两片发送，服务器应正确响应 200', async () => {
        const res = await client.sendChunks([
            'GET / HTTP/1.1\r\n',
            'Host: localhost\r\nConnection: close\r\n\r\n'
        ], 50);
        expect(res.status).toBe(200);
    });

    test('请求头逐行分片发送，服务器应正确响应 200', async () => {
        const res = await client.sendChunks([
            'GET / HTTP/1.1\r\n',
            'Host: localhost\r\n',
            'Connection: close\r\n',
            '\r\n',
        ], 100);
        expect(res.status).toBe(200);
    });

    test('POST body 分片发送，服务器应正确响应', async () => {
        const body = 'hello=world';
        const res = await client.sendChunks([
            `POST / HTTP/1.1\r\n`,
            `Host: localhost\r\n`,
            `Content-Type: application/x-www-form-urlencoded\r\n`,
            `Content-Length: ${Buffer.byteLength(body)}\r\n`,
            `Connection: close\r\n`,
            `\r\n`,
            body,
        ], 50);
        expect([200, 201, 204, 405]).toContain(res.status);
    });

    test('极慢客户端(200ms 间隔），服务器应在超时前正确响应', async () => {
        const res = await client.sendChunks([
            'GET / HTTP/1.1\r\n',
            'Host: localhost\r\n',
            'Connection: close\r\n',
            '\r\n',
        ], 200);
        expect(res.status).toBe(200);
    });
});

describe('边界情况', () => {
    test('空 body 的 POST', async () => {
        const res = await client.post('/', '');
        expect([200, 201, 204, 400, 405, 411]).toContain(res.status);
    });

    test('超大 URI 应返回 414', async () => {
        const res = await client.get('/' + 'a'.repeat(8192));
        expect(res.status).toBe(414);
    });

    test('不支持的方法应返回 405 或 501', async () => {
        const res = await client.request('PATCH', '/');
        expect([405, 501]).toContain(res.status);
    });
});

describe('状态码专项', () => {
    // test('GET /redirect 应返回 301 且有 Location 头', async () => {
    //     const res = await client.get('/redirect');
    //     expect(res.status).toBe(301);
    //     expect(res.headers).toHaveProperty('location');
    //     expect(res.headers['location']).toBe('http://another-site.com');
    // });

    test('POST 超出 client_max_body_size 应返回 413', async () => {
        // cat.com 的限制是 10m，对 localhost 服务器发超大 body
        const bigBody = 'x'.repeat(11 * 1024 * 1024); // 11MB
        const res = await client.post('/uploads', bigBody, {
            'Content-Type': 'text/plain',
        });
        expect(res.status).toBe(413);
    });

    test('DELETE /uploads 应返回合法状态码', async () => {
        const res = await client.request('DELETE', '/uploads');
        expect([200, 204, 404]).toContain(res.status);
    });
});

describe('CGI 测试', () => {
    test('GET /cgi-php/hello.php 应返回 200 且包含 PHP 输出', async () => {
        const res = await client.get('/cgi-php/hello.php');
        expect(res.status).toBe(200);
        expect(res.body.toString()).toContain('Hello from PHP CGI');
        expect(res.body.toString()).toContain('Method: GET');
    });

    test('GET /cgi-php/hello.php?name=test 应正确传递 query string', async () => {
        const res = await client.get('/cgi-php/hello.php?name=test');
        expect(res.status).toBe(200);
        expect(res.body.toString()).toContain('Query: name=test');
    });

    test('POST /cgi-php/hello.php 应正确传递 method', async () => {
        const res = await client.post('/cgi-php/hello.php', 'data=123', {
            'Content-Type': 'application/x-www-form-urlencoded',
        });
        expect(res.status).toBe(200);
        expect(res.body.toString()).toContain('Method: POST');
    });
});