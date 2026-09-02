import type { Task } from '../api/types';
import { PRIORITY_LABEL, SUBJECT_LABEL, TASK_STATUS_LABEL } from '../lib/format';

export interface TaskListProps {
  tasks: Task[];
  onEdit?: (task: Task) => void;
}

export function TaskList({ tasks, onEdit }: TaskListProps) {
  if (tasks.length === 0) {
    return <p className="empty-hint">今日还没有任务，点“创建任务”添加。</p>;
  }
  return (
    <ul className="task-list" aria-label="今日任务列表">
      {tasks.map((t) => (
        <li key={t.task_id} className="task-row">
          <div className="task-main">
            <span className="subject-chip" data-subject={t.subject}>
              {SUBJECT_LABEL[t.subject] ?? t.subject}
            </span>
            <span className="task-title">{t.title}</span>
            <span className="task-meta">
              {t.estimated_minutes} 分钟 · {PRIORITY_LABEL[t.priority] ?? t.priority}优先级
            </span>
          </div>
          <div className="task-side">
            <span className="task-status" data-status={t.status}>
              {TASK_STATUS_LABEL[t.status] ?? t.status}
            </span>
            {onEdit ? (
              <button type="button" className="link" onClick={() => onEdit(t)}>
                编辑
              </button>
            ) : null}
          </div>
        </li>
      ))}
    </ul>
  );
}
