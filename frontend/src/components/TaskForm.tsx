import { useState } from 'react';
import {
  PRIORITIES,
  PRIORITY_LABEL,
  SUBJECTS,
  SUBJECT_LABEL,
  TaskFormErrors,
  TaskFormValues,
  hasErrors,
  validateTaskForm,
} from '../lib/format';
import type { Task } from '../api/types';

export interface TaskFormProps {
  initial?: Task | null;
  busy?: boolean;
  onSubmit: (values: TaskFormValues) => void;
  onCancel?: () => void;
}

const EMPTY: TaskFormValues = {
  title: '',
  subject: 'math',
  estimated_minutes: 20,
  priority: 'medium',
};

export function TaskForm({ initial, busy = false, onSubmit, onCancel }: TaskFormProps) {
  const [values, setValues] = useState<TaskFormValues>(() =>
    initial
      ? {
          title: initial.title,
          subject: initial.subject,
          estimated_minutes: initial.estimated_minutes,
          priority: initial.priority,
        }
      : EMPTY,
  );
  const [errors, setErrors] = useState<TaskFormErrors>({});
  const editing = initial !== undefined && initial !== null;

  function submit(e: React.FormEvent) {
    e.preventDefault();
    const errs = validateTaskForm(values);
    setErrors(errs);
    if (!hasErrors(errs)) {
      onSubmit({
        ...values,
        title: values.title.trim(),
        estimated_minutes: Number(values.estimated_minutes),
      });
    }
  }

  return (
    <form onSubmit={submit} noValidate aria-label={editing ? '编辑任务表单' : '新建任务表单'}>
      <div className="field">
        <label htmlFor="tf-title">科目</label>
        <select
          id="tf-title"
          aria-label="科目"
          value={values.subject}
          onChange={(e) => setValues({ ...values, subject: e.target.value })}
        >
          {SUBJECTS.map((s) => (
            <option key={s} value={s}>
              {SUBJECT_LABEL[s]}
            </option>
          ))}
        </select>
      </div>
      <div className="field">
        <label htmlFor="tf-task">任务内容</label>
        <input
          id="tf-task"
          type="text"
          value={values.title}
          maxLength={200}
          onChange={(e) => setValues({ ...values, title: e.target.value })}
          aria-invalid={errors.title ? true : undefined}
        />
        {errors.title ? <p className="error-text" role="alert">{errors.title}</p> : null}
      </div>
      <div className="field">
        <label htmlFor="tf-minutes">预计分钟</label>
        <input
          id="tf-minutes"
          type="number"
          min={1}
          max={600}
          value={values.estimated_minutes}
          onChange={(e) =>
            setValues({
              ...values,
              estimated_minutes: e.target.value === '' ? '' : Number(e.target.value),
            })
          }
          aria-invalid={errors.estimated_minutes ? true : undefined}
        />
        {errors.estimated_minutes ? (
          <p className="error-text" role="alert">{errors.estimated_minutes}</p>
        ) : null}
      </div>
      <div className="field">
        <label htmlFor="tf-priority">优先级</label>
        <select
          id="tf-priority"
          aria-label="优先级"
          value={values.priority}
          onChange={(e) => setValues({ ...values, priority: e.target.value })}
        >
          {PRIORITIES.map((p) => (
            <option key={p} value={p}>
              {PRIORITY_LABEL[p]}
            </option>
          ))}
        </select>
      </div>
      <div className="form-actions">
        <button type="submit" disabled={busy} className="primary">
          {editing ? '保存修改' : '创建任务'}
        </button>
        {onCancel ? (
          <button type="button" onClick={onCancel}>
            取消
          </button>
        ) : null}
      </div>
    </form>
  );
}
