CREATE TABLE tasks (
  id INTEGER PRIMARY KEY,
  title VARCHAR,
  priority VARCHAR,
  due_date INTEGER,
  status VARCHAR

);
CREATE TABLE users (
  id INTEGER PRIMARY KEY,
  name VARCHAR
);

CREATE TABLE task_assignments (
  assignment_id INTEGER PRIMARY KEY,
  task_id INTEGER,
  user_id INTEGER,
  FOREIGN KEY (task_id) REFERENCES tasks (id),
  FOREIGN KEY (user_id) REFERENCES users (id)
)
