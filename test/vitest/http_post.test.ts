import { test, expect, beforeAll } from 'vitest';
import { TestClient } from './TestClient.js';

const client = new TestClient(8080);

test('POST body parsing test', async () => {
  const res = await client.post('/directory', 'name=42student');
  expect(res).toContain('201 Created');
});

test('POST with custom headers', async () => {
  const res = await client.post('/directory', 'data', { 'X-Custom': 'test' });
  expect(res).toContain('201 Created');
});