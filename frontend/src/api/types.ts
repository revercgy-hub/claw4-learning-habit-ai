// Claw4 parent PWA — API types mirroring the family backend schemas
// (backend/app/schemas.py). Field names stay snake_case to match the JSON
// contract 1:1; interface drift is caught by the integration tests (CP7).
export interface Task {
  task_id: string;
  title: string;
  subject: string;
  estimated_minutes: number;
  priority: string; // high | medium | low
  status: string; // pending | ready | in_progress | paused | completed | skipped
  scheduled_date: string;
  version: number;
}

export interface TaskCreateInput {
  subject: string;
  title: string;
  estimated_minutes: number;
  priority: string;
}

export interface TaskUpdateInput {
  subject?: string;
  title?: string;
  estimated_minutes?: number;
  priority?: string;
}

export interface TodayTasks {
  date: string;
  tasks: Task[];
}

export interface Dashboard {
  date: string;
  planned_tasks: number;
  completed_tasks: number;
  completion_rate: number; // 0-100
  focus_minutes: number;
  current_activity: string | null;
}

export interface StudySession {
  session_id: string;
  task_id: string;
  task_title: string;
  status: string;
  completion_type: string | null;
  actual_seconds: number;
  pause_count: number;
  pause_seconds: number;
  xp: number;
  started_at: number;
  finished_at: number | null;
}

export interface Device {
  device_id: string;
  model: string;
  fw_version: string;
  online: boolean;
  battery_percent: number | null; // null == unknown (never fabricated)
  last_seen_at: number | null;
  last_sync_at: number | null;
  last_acked_sequence: number;
}

export interface ParentMe {
  parent_id: string;
  stub: string;
  child_ids: string[];
}
