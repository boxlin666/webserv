import { describe, test, expect } from 'vitest';
import { TestClient } from './TestClient.js';

const client = new TestClient(8080);

describe('Concurrency Tests', () => {
    test('50 concurrent GET / requests should all return 200', async () => {
        const tasks = Array.from({ length: 50 }, () => client.get('/'));
        const results = await Promise.allSettled(tasks);

        const fulfilled = results.filter(r => r.status === 'fulfilled');
        const rejected  = results.filter(r => r.status === 'rejected');

        console.log(`Succeeded: ${fulfilled.length}, Failed: ${rejected.length}`);
        if (rejected.length > 0) {
            rejected.forEach(r => console.log('Failure reason:', (r as PromiseRejectedResult).reason));
        }

        expect(rejected.length).toBe(0);
        fulfilled.forEach(r => {
            expect((r as PromiseFulfilledResult<{ status: number }>).value.status).toBe(200);
        });
    }, 15000);

    test('50 concurrent GET / requests should complete within a reasonable time', async () => {
        const start = Date.now();
        const tasks = Array.from({ length: 50 }, () => client.get('/'));
        await Promise.all(tasks);
        const elapsed = Date.now() - start;

        console.log(`50 concurrent requests completed in: ${elapsed}ms`);
        expect(elapsed).toBeLessThan(10000);
    }, 15000);

    test('Mixed concurrency: simultaneous GET and POST requests', async () => {
        const gets  = Array.from({ length: 25 }, () => client.get('/'));
        const posts = Array.from({ length: 25 }, () =>
            client.post('/uploads', 'concurrent=true', { 'Content-Type': 'text/plain' })
        );

        const results = await Promise.allSettled([...gets, ...posts]);
        const rejected = results.filter(r => r.status === 'rejected');

        console.log(`Mixed concurrency failures: ${rejected.length}`);
        expect(rejected.length).toBe(0);
    }, 15000);
});
