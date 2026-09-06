import { afterEach, expect, it, vi } from 'vitest';
import { cleanup, render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { App } from './App';

afterEach(() => {
  cleanup();
  sessionStorage.clear();
  vi.unstubAllGlobals();
});

it('restores authorization on reload and removes it on logout', async () => {
  sessionStorage.setItem('claw4_parent_token', 'synthetic-session');
  const request = vi.fn(async (_url: string, init: RequestInit) => {
    const authorized = (init.headers as Record<string, string>).Authorization === 'Bearer synthetic-session';
    return new Response(JSON.stringify(authorized ? {
      date: '2026-09-05', planned_tasks: 2, completed_tasks: 1,
      completion_rate: 50, focus_minutes: 20, current_activity: null,
    } : { detail: 'unauthorized' }), { status: authorized ? 200 : 401 });
  });
  vi.stubGlobal('fetch', request);
  render(<App />);
  await screen.findByText('50%');
  expect(screen.queryByRole('alert')).not.toBeInTheDocument();
  await userEvent.click(screen.getByRole('button', { name: '退出' }));
  expect(sessionStorage.getItem('claw4_parent_token')).toBeNull();
  cleanup();
  request.mockClear();
  render(<App />);
  await screen.findByRole('button', { name: '进入' });
  expect(request).not.toHaveBeenCalled();
});
