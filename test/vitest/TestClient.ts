import net from 'net';

export interface HttpResponse {
    status: number;
    headers: Record<string, string>;
    body: Buffer;
    raw: string;
}

export class TestClient {
    constructor(
        private port: number = 8080,
        private host: string = '127.0.0.1',
        private timeoutMs: number = 20000
    ) { }

    private parseResponse(raw: Buffer): HttpResponse {
        // 在 Buffer 层面找分隔符，避免二进制 body 被破坏
        const sep = Buffer.from('\r\n\r\n');
        const sepIdx = raw.indexOf(sep);

        const head = raw.subarray(0, sepIdx).toString('utf8');
        const body = raw.subarray(sepIdx + 4); // 跳过 \r\n\r\n

        const lines = head.split('\r\n');
        const statusLine = lines[0] ?? '';
        const status = parseInt(statusLine.split(' ')[1] ?? '0', 10);

        const headers: Record<string, string> = {};
        lines.slice(1).forEach(line => {
            const colonIdx = line.indexOf(': ');
            if (colonIdx !== -1) {
                const key = line.slice(0, colonIdx).toLowerCase();
                const val = line.slice(colonIdx + 2);
                headers[key] = val;
            }
        });

        return { status, headers, body, raw: raw.toString('utf8') };
    }

    // 收集原始 Buffer，而非字符串，保留二进制完整性
    private rawRequest(request: string, halfClose = false): Promise<HttpResponse> {
        return new Promise((resolve, reject) => {
            const client = net.createConnection({ port: this.port, host: this.host });
            const chunks: Buffer[] = [];

            client.setTimeout(this.timeoutMs, () => {
                client.destroy();
                reject(new Error(`Socket timeout after ${this.timeoutMs}ms`));
            });

            client.on('connect', () => {
                client.write(request);
                if (halfClose) {
                    setTimeout(() => client.end(), 100);
                }
            });

            client.on('data', (chunk: Buffer) => chunks.push(chunk));
            client.on('end', () => resolve(this.parseResponse(Buffer.concat(chunks))));
            client.on('error', reject);
        });
    }

    async get(path: string, headers: Record<string, string> = {}): Promise<HttpResponse> {
        const extraHeaders = Object.entries({ Host: 'localhost', Connection: 'close', ...headers })
            .map(([k, v]) => `${k}: ${v}`).join('\r\n');

        return this.rawRequest(`GET ${path} HTTP/1.1\r\n${extraHeaders}\r\n\r\n`);
    }

    async post(path: string, body: string, headers: Record<string, string> = {}): Promise<HttpResponse> {
        const allHeaders = Object.entries({
            Host: 'localhost',
            'Content-Length': Buffer.byteLength(body), // 修复：用字节长度
            Connection: 'close',
            ...headers,
        }).map(([k, v]) => `${k}: ${v}`).join('\r\n');

        return this.rawRequest(`POST ${path} HTTP/1.1\r\n${allHeaders}\r\n\r\n${body}`);
    }

    // 支持自定义 method，方便测试 PUT/DELETE/非标请求
    async request(method: string, path: string, body = '', headers: Record<string, string> = {}): Promise<HttpResponse> {
        const allHeaders = Object.entries({
            Host: 'localhost',
            Connection: 'close',
            ...(body ? { 'Content-Length': Buffer.byteLength(body) } : {}),
            ...headers, // 调用方可以用这里覆盖 Content-Length
        }).map(([k, v]) => `${k}: ${v}`).join('\r\n');

        const raw = `${method} ${path} HTTP/1.1\r\n${allHeaders}\r\n\r\n${body}`;
        // Content-Length 被覆盖时，说明是故意构造的畸形请求，需要 halfClose 触发服务器响应
        const isManualLength = 'Content-Length' in headers;
        return this.rawRequest(raw, isManualLength);
    }

    async sendChunks(chunks: string[], intervalMs = 50): Promise<HttpResponse> {
        return new Promise((resolve, reject) => {
            const client = net.createConnection({ port: this.port, host: this.host });
            const buffers: Buffer[] = [];

            // sendChunks 也应该有超时保护
            const timer = setTimeout(() => {
                client.destroy();
                reject(new Error(`sendChunks timeout after ${this.timeoutMs}ms`));
            }, this.timeoutMs);

            client.on('connect', async () => {
                for (const chunk of chunks) {
                    client.write(chunk);
                    await new Promise(r => setTimeout(r, intervalMs));
                }
                // chunks 发完后才 end，让服务器知道请求结束
                client.end();
            });

            client.on('data', (chunk: Buffer) => buffers.push(chunk));
            client.on('end', () => {
                clearTimeout(timer);
                resolve(this.parseResponse(Buffer.concat(buffers)));
            });
            client.on('error', (err) => {
                clearTimeout(timer);
                reject(err);
            });
        });
    }
}