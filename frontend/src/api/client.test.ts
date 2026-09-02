import { describe, expect, it, vi } from 'vitest';
import { ApiClient, ApiError } from './client';

function jsonResponse(body: unknown, status = 200): Response {
  return new Response(JSON.stringify(body), {
    status,
    headers: { 'Content-Type': 'application/json' },
  });
}

describe('ApiClient', () => {
  it('uses the configured base URL without trailing slash', async () => {
    const fetchImpl = vi.fn().mockResolvedValue(jsonResponse({ status: 'ok' }));
    const c = new ApiClient({ baseUrl: 'http://127.0.0.1:8000/', fetchImpl });
    await c.me();
    expect(fetchImpl).toHaveBeenCalledWith(
      'http://127.0.0.1:8000/api/v1/parents/me',
      expect.objectContaining({}),
    );
  });

  it('attaches the Bearer token when set', async () => {
    const fetchImpl = vi.fn().mockResolvedValue(jsonResponse({ parent_id: 'parent-1' }));
    const c = new ApiClient({ baseUrl: 'http://x', token: 'tok-1', fetchImpl });
    await c.me();
    const init = fetchImpl.mock.calls[0][1] as RequestInit;
    expect((init.headers as Record<string, string>).Authorization).toBe('Bearer tok-1');
  });

  it('sends JSON bodies for create/update', async () => {
    const fetchImpl = vi.fn().mockResolvedValue(
      jsonResponse({ task_id: 't1', title: 'A', subject: 'math', estimated_minutes: 10, priority: 'medium', status: 'pending', scheduled_date: '2026-01-01', version: 1 }, 201),
    );
    const c = new ApiClient({ baseUrl: 'http://x', token: 't', fetchImpl });
    await c.createTask('child-1', {
      subject: 'math', title: 'A', estimated_minutes: 10, priority: 'medium',
    });
    const init = fetchImpl.mock.calls[0][1] as RequestInit;
    expect(init.method).toBe('POST');
    expect(JSON.parse(init.body as string)).toMatchObject({ title: 'A' });
    expect((init.headers as Record<string, string>)['Content-Type']).toBe('application/json');
  });

  it('throws ApiError with the server detail on non-2xx', async () => {
    const fetchImpl = vi.fn().mockResolvedValue(
      new Response(JSON.stringify({ detail: 'operation_not_allowed' }), {
        status: 403,
        headers: { 'Content-Type': 'application/json' },
      }),
    );
    const c = new ApiClient({ baseUrl: 'http://x', token: 't', fetchImpl });
    await expect(c.studySessions('child-9')).rejects.toMatchObject({
      status: 403,
      message: 'operation_not_allowed',
    });
  });

  it('maps ApiError subclass properly', async () => {
    const fetchImpl = vi.fn().mockResolvedValue(
      new Response('oops', { status: 500 }),
    );
    const c = new ApiClient({ baseUrl: 'http://x', token: 't', fetchImpl });
    try {
      await c.dashboard();
      expect.unreachable();
    } catch (e) {
      expect(e).toBeInstanceOf(ApiError);
      expect((e as ApiError).status).toBe(500);
    }
  });

  it('uses VITE_API_BASE_URL env as fallback base', () => {
    const c = new ApiClient({ token: 'x' });
    expect(c.baseUrl).toMatch(/^http/);
  });
});
