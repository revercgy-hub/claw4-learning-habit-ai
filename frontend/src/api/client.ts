// Claw4 parent PWA — API client (host MVP).
//
// The base URL comes from the environment (VITE_API_BASE_URL) and never from
// hard-coded code. The development-session parent token is entered by the
// user on the login screen (kept in sessionStorage only) — it is never baked
// into the bundle or written into source. No device is ever contacted
// directly and no AI provider is used.
import type {
  Dashboard,
  Device,
  ParentMe,
  StudySession,
  Task,
  TaskCreateInput,
  TaskUpdateInput,
  TodayTasks,
} from './types';

const BASE_URL = (import.meta.env.VITE_API_BASE_URL as string | undefined) ??
  'http://127.0.0.1:8000';

export class ApiError extends Error {
  readonly status: number;
  constructor(status: number, message: string) {
    super(message);
    this.status = status;
  }
}

export interface ApiClientOptions {
  baseUrl?: string;
  token?: string;
  fetchImpl?: typeof fetch;
}

export class ApiClient {
  readonly baseUrl: string;
  private token: string;
  private fetchImpl: typeof fetch;

  constructor(opts: ApiClientOptions = {}) {
    this.baseUrl = (opts.baseUrl ?? BASE_URL).replace(/\/$/, '');
    this.token = opts.token ?? '';
    this.fetchImpl = opts.fetchImpl ?? fetch;
  }

  setToken(token: string): void {
    this.token = token;
  }

  private async request<T>(path: string, init: RequestInit = {}): Promise<T> {
    const headers: Record<string, string> = {
      ...(init.headers as Record<string, string> | undefined),
    };
    if (this.token) headers['Authorization'] = `Bearer ${this.token}`;
    if (init.body) headers['Content-Type'] = 'application/json';
    const resp = await this.fetchImpl(`${this.baseUrl}${path}`, {
      ...init,
      headers,
    });
    if (!resp.ok) {
      let detail = `HTTP ${resp.status}`;
      try {
        const body = (await resp.json()) as { detail?: string };
        if (body.detail) detail = body.detail;
      } catch {
        // non-JSON error body
      }
      throw new ApiError(resp.status, detail);
    }
    return (await resp.json()) as T;
  }

  me(): Promise<ParentMe> {
    return this.request<ParentMe>('/api/v1/parents/me');
  }

  dashboard(): Promise<Dashboard> {
    return this.request<Dashboard>('/api/v1/parents/me/dashboard');
  }

  tasksToday(childId: string): Promise<TodayTasks> {
    return this.request<TodayTasks>(
      `/api/v1/parents/me/children/${encodeURIComponent(childId)}/tasks/today`,
    );
  }

  createTask(childId: string, input: TaskCreateInput): Promise<Task> {
    return this.request<Task>(
      `/api/v1/parents/me/children/${encodeURIComponent(childId)}/tasks`,
      { method: 'POST', body: JSON.stringify(input) },
    );
  }

  updateTask(childId: string, taskId: string, input: TaskUpdateInput): Promise<Task> {
    return this.request<Task>(
      `/api/v1/parents/me/children/${encodeURIComponent(childId)}/tasks/${encodeURIComponent(taskId)}`,
      { method: 'PATCH', body: JSON.stringify(input) },
    );
  }

  studySessions(childId: string): Promise<StudySession[]> {
    return this.request<StudySession[]>(
      `/api/v1/parents/me/children/${encodeURIComponent(childId)}/study-sessions`,
    );
  }

  devices(): Promise<Device[]> {
    return this.request<Device[]>('/api/v1/parents/me/devices');
  }
}
