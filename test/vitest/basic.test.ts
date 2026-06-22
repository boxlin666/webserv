import { describe, test, expect, beforeAll } from 'vitest';
import { TestClient } from './TestClient.js';

const client = new TestClient(8080);

describe('GET Requests', () => {
    test('GET / should return 200', async () => {
        const res = await client.get('/');
        expect(res.status).toBe(200);
    });

    test('GET non-existent path should return 404', async () => {
        const res = await client.get('/this-does-not-exist');
        expect(res.status).toBe(404);
    });

    test('Response headers should contain Content-Type', async () => {
        const res = await client.get('/');
        expect(res.headers).toHaveProperty('content-type');
    });

    test('Response body should not be empty', async () => {
        const res = await client.get('/');
        expect(res.body.length).toBeGreaterThan(0);
    });
});

describe('POST Requests', () => {
    test('POST / should return a valid status code', async () => {
        const res = await client.post('/', 'hello=world', {
            'Content-Type': 'application/x-www-form-urlencoded',
        });
        expect([200, 201, 204, 405]).toContain(res.status);
    });

    test('Mismatched Content-Length should return 408', async () => {
        const res = await client.request('POST', '/', 'hello', {
            'Content-Type': 'text/plain',
            'Content-Length': '999',
        });
        expect(res.status).toBe(400);
    }, 1200);
});

describe('Fragmented Requests (Slow Client Simulation)', () => {
    test('Request headers split into two chunks should return 200', async () => {
        const res = await client.sendChunks([
            'GET / HTTP/1.1\r\n',
            'Host: localhost\r\nConnection: close\r\n\r\n'
        ], 50);
        expect(res.status).toBe(200);
    });

    test('Request headers sent line by line should return 200', async () => {
        const res = await client.sendChunks([
            'GET / HTTP/1.1\r\n',
            'Host: localhost\r\n',
            'Connection: close\r\n',
            '\r\n',
        ], 100);
        expect(res.status).toBe(200);
    });

    test('POST body sent in fragments should return a valid status code', async () => {
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

    test('Very slow client (200ms interval) should get a valid response before timeout', async () => {
        const res = await client.sendChunks([
            'GET / HTTP/1.1\r\n',
            'Host: localhost\r\n',
            'Connection: close\r\n',
            '\r\n',
        ], 200);
        expect(res.status).toBe(200);
    });
});

describe('Edge Cases', () => {
    test('POST with empty body should return a valid status code', async () => {
        const res = await client.post('/', '');
        expect([200, 201, 204, 400, 405, 411]).toContain(res.status);
    });

    test('Oversized URI should return 414', async () => {
        const res = await client.get('/' + 'a'.repeat(8192));
        expect(res.status).toBe(414);
    });

    test('Unsupported method should return 405 or 501', async () => {
        const res = await client.request('PATCH', '/');
        expect([405, 501]).toContain(res.status);
    });
});

describe('Status Code Tests', () => {
    test('GET /redirect_ext should return 301 with a Location header', async () => {
        const res = await client.get('/redirect_ext');
        expect(res.status).toBe(301);
        expect(res.headers).toHaveProperty('location');
        expect(res.headers['location']).toBe('http://google.com');
    });

    test('POST exceeding client_max_body_size should return 413', async () => {
        const bigBody = 'x'.repeat(11 * 1024 * 1024); // 11MB
        const res = await client.post('/uploads', bigBody, {
            'Content-Type': 'text/plain',
        });
        expect(res.status).toBe(413);
    });

    test('DELETE /uploads should return a valid status code', async () => {
        const res = await client.request('DELETE', '/uploads');
        expect([200, 204, 404]).toContain(res.status);
    });
});

describe('CGI Tests', () => {
    test('GET /cgi-php/hello.php should return 200 with PHP output', async () => {
        const res = await client.get('/cgi-php/hello.php');
        expect(res.status).toBe(200);
        expect(res.body.toString()).toContain('Hello from PHP CGI');
        expect(res.body.toString()).toContain('Method: GET');
    });

    test('GET /cgi-php/hello.php?name=test should correctly pass query string', async () => {
        const res = await client.get('/cgi-php/hello.php?name=test');
        expect(res.status).toBe(200);
        expect(res.body.toString()).toContain('Query: name=test');
    });

    test('POST /cgi-php/hello.php should correctly pass method', async () => {
        const res = await client.post('/cgi-php/hello.php', 'data=123', {
            'Content-Type': 'application/x-www-form-urlencoded',
        });
        expect(res.status).toBe(200);
        expect(res.body.toString()).toContain('Method: POST');
    });
});