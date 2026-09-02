import type { Dashboard, Device, StudySession } from '../api/types';
import { batteryLabel, formatDuration } from '../lib/format';

export interface StatCardProps {
  label: string;
  value: string | number;
}

export function StatCard({ label, value }: StatCardProps) {
  return (
    <div className="stat-card">
      <span className="stat-value">{value}</span>
      <span className="stat-label">{label}</span>
    </div>
  );
}

export interface DashboardCardsProps {
  data: Dashboard;
}

export function DashboardCards({ data }: DashboardCardsProps) {
  return (
    <div className="stat-grid" aria-label="学习概况">
      <StatCard label="计划任务" value={data.planned_tasks} />
      <StatCard label="已完成" value={data.completed_tasks} />
      <StatCard label="完成率" value={`${data.completion_rate}%`} />
      <StatCard label="专注分钟" value={data.focus_minutes} />
      <div className="current-activity">
        当前学习：{data.current_activity ?? '空闲'}
      </div>
    </div>
  );
}

export interface RecordRowProps {
  record: StudySession;
}

export function RecordRow({ record }: RecordRowProps) {
  const done = record.status === 'completed';
  const when = done
    ? new Date((record.finished_at ?? record.started_at) * 1000).toLocaleString('zh-CN')
    : new Date(record.started_at * 1000).toLocaleString('zh-CN');
  return (
    <li className="record-row">
      <span className="record-task">{record.task_title || record.task_id}</span>
      <span className="record-time">{when}</span>
      <span className="record-duration">{formatDuration(record.actual_seconds)}</span>
      <span className="record-pauses">暂停 {record.pause_count} 次</span>
      <span className="record-xp">+{record.xp} XP</span>
      <span className="record-status" data-status={record.status}>
        {done ? '已完成' : record.status === 'running' ? '进行中' : record.status}
      </span>
    </li>
  );
}

export interface DeviceRowProps {
  device: Device;
}

export function DeviceRow({ device }: DeviceRowProps) {
  const lastSync =
    device.last_sync_at === null || device.last_sync_at === undefined
      ? '从未同步'
      : new Date(device.last_sync_at * 1000).toLocaleString('zh-CN');
  return (
    <li className="device-row">
      <span className="device-online" data-online={device.online}>
        {device.online ? '在线' : '离线'}
      </span>
      <span className="device-name">{device.model} ({device.device_id.slice(0, 8)}…)</span>
      <span className="device-battery">电量 {batteryLabel(device.battery_percent)}</span>
      <span className="device-fw">固件 {device.fw_version}</span>
      <span className="device-sync">最近同步：{lastSync}</span>
    </li>
  );
}
