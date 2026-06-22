import { describe, test, expect } from 'vitest';
import { TestClient } from './TestClient.js';

const client = new TestClient(8080);

describe('POST Body Parsing', () => {
    test('POST form data to /uploads should return 201', async () => {
        const res = await client.post('/uploads', 'name=42student', {
            'Content-Type': 'application/x-www-form-urlencoded',
        });
        expect(res.status).toBe(201);
    });

    test('POST with custom headers to /uploads should return 201', async () => {
        const res = await client.post('/uploads', 'data', {
            'Content-Type': 'text/plain',
            'X-Custom': 'test',
        });
        expect(res.status).toBe(201);
    });

    test('POST to a restricted path should return 405', async () => {
        // / only allows GET and HEAD, POST should be rejected
        const res = await client.post('/', 'name=42student');
        expect(res.status).toBe(405);
    });
});