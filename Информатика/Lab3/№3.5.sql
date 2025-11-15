SELECT title, priority, due_date FROM tasks;

SELECT * FROM task_assignments
    JOIN users
	ON users.id = task_assignments.user_id
	JOIN tasks
	ON tasks.id = task_assignments.task_id
WHERE users.id = 3;

SELECT title, due_date, status FROM tasks
WHERE priority = 'Высокий' AND due_date <= 7;

SELECT COUNT(*) FROM tasks WHERE status = 'Выполнена';
SELECT COUNT(*) FROM tasks WHERE status = 'Отложена';
SELECT COUNT(*) FROM tasks WHERE status = 'В процессе';

