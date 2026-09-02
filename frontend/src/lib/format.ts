// Pure formatting/validation helpers (unit-testable, no React/DOM).
export const SUBJECTS = ['math', 'chinese', 'english', 'science', 'other'] as const;
export const PRIORITIES = ['high', 'medium', 'low'] as const;

export type Subject = (typeof SUBJECTS)[number];
export type Priority = (typeof PRIORITIES)[number];

export const SUBJECT_LABEL: Record<string, string> = {
  math: '数学',
  chinese: '语文',
  english: '英语',
  science: '科学',
  other: '其他',
};

export const PRIORITY_LABEL: Record<string, string> = {
  high: '高',
  medium: '中',
  low: '低',
};

export const TASK_STATUS_LABEL: Record<string, string> = {
  pending: '待开始',
  ready: '可开始',
  in_progress: '进行中',
  paused: '已暂停',
  completed: '已完成',
  skipped: '已跳过',
};

/** seconds -> human readable "X 分 Y 秒" (empty for 0). */
export function formatDuration(totalSeconds: number): string {
  const s = Math.max(0, Math.floor(totalSeconds));
  const minutes = Math.floor(s / 60);
  const seconds = s % 60;
  if (minutes <= 0 && seconds <= 0) return '0 秒';
  const parts: string[] = [];
  if (minutes > 0) parts.push(`${minutes} 分`);
  if (seconds > 0) parts.push(`${seconds} 秒`);
  return parts.join(' ');
}

/** Battery label: null/unknown is shown as "未知", never a fabricated number. */
export function batteryLabel(percent: number | null | undefined): string {
  if (percent === null || percent === undefined) return '未知';
  return `${percent}%`;
}

export interface TaskFormValues {
  title: string;
  subject: string;
  estimated_minutes: number | '';
  priority: string;
}

export interface TaskFormErrors {
  title?: string;
  subject?: string;
  estimated_minutes?: string;
}

/** MVP validation for the create/edit task form. */
export function validateTaskForm(v: TaskFormValues): TaskFormErrors {
  const errors: TaskFormErrors = {};
  const title = v.title.trim();
  if (!title) errors.title = '请输入任务内容';
  else if (title.length > 200) errors.title = '任务内容不能超过 200 字';
  if (!v.subject) errors.subject = '请选择科目';
  if (v.estimated_minutes === '' || Number.isNaN(Number(v.estimated_minutes))) {
    errors.estimated_minutes = '请输入预计分钟数';
  } else {
    const m = Number(v.estimated_minutes);
    if (!Number.isInteger(m) || m < 1 || m > 600) {
      errors.estimated_minutes = '预计分钟数需为 1–600 的整数';
    }
  }
  return errors;
}

export function hasErrors(e: TaskFormErrors): boolean {
  return Object.values(e).some((v) => v !== undefined);
}
