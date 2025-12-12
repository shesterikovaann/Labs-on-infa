CREATE TABLE students (
    student_id SERIAL PRIMARY KEY,
    full_name VARCHAR(100) NOT NULL,
    group_number VARCHAR(20) NOT NULL
);

CREATE TABLE subjects (
    subject_id SERIAL PRIMARY KEY,
    subject_name VARCHAR(100) NOT NULL
);

CREATE TABLE grades (
    grade_id SERIAL PRIMARY KEY,
    student_id INTEGER REFERENCES students(student_id) ON DELETE CASCADE,
    subject_id INTEGER REFERENCES subjects(subject_id) ON DELETE CASCADE,
    grade INTEGER CHECK (grade BETWEEN 1 AND 5)
);

CREATE TABLE attendance (
    attendance_id SERIAL PRIMARY KEY,
    student_id INTEGER REFERENCES students(student_id) ON DELETE CASCADE,
    date_attended DATE NOT NULL,
    status VARCHAR(10) CHECK (status IN ('присутствовал', 'отсутствовал', 'опоздал'))
);

CREATE TABLE notes (
    note_id SERIAL PRIMARY KEY,
    student_id INTEGER REFERENCES students(student_id) ON DELETE CASCADE,
    note_text TEXT NOT NULL
);


INSERT INTO students (full_name, group_number) VALUES
('Иванов Иван Иванович', 'ИВТ-101'),
('Петров Петр Петрович', 'ИВТ-101'),
('Сидорова Мария Сергеевна', 'ИВТ-101'),
('Кузнецов Алексей Владимирович', 'ИТ-101'),
('Смирнова Екатерина Дмитриевна', 'ИТ-101'),
('Федоров Денис Олегович', 'ИТ-101');


INSERT INTO subjects (subject_name) VALUES
('Матан'),
('Ангем'),
('Инфа');


DO $$
DECLARE
    math_id INTEGER;
    geom_id INTEGER;
    info_id INTEGER;
    student_rec RECORD;
BEGIN

    SELECT subject_id INTO math_id FROM subjects WHERE subject_name = 'Матан';
    SELECT subject_id INTO geom_id FROM subjects WHERE subject_name = 'Ангем';
    SELECT subject_id INTO info_id FROM subjects WHERE subject_name = 'Инфа';
    
    FOR student_rec IN SELECT student_id FROM students ORDER BY student_id
    LOOP

        INSERT INTO grades (student_id, subject_id, grade) VALUES
        (student_rec.student_id, math_id, (RANDOM() * 2 + 3)::INTEGER),
        (student_rec.student_id, math_id, (RANDOM() * 2 + 3)::INTEGER);
        

        INSERT INTO grades (student_id, subject_id, grade) VALUES
        (student_rec.student_id, geom_id, (RANDOM() * 2 + 3)::INTEGER),
        (student_rec.student_id, geom_id, (RANDOM() * 2 + 3)::INTEGER);
        

        INSERT INTO grades (student_id, subject_id, grade) VALUES
        (student_rec.student_id, info_id, (RANDOM() * 2 + 3)::INTEGER),
        (student_rec.student_id, info_id, (RANDOM() * 2 + 3)::INTEGER);
    END LOOP;
END $$;

DO $$
DECLARE
    student_rec RECORD;
BEGIN
    FOR student_rec IN SELECT student_id FROM students ORDER BY student_id
    LOOP
        -- Посещаемость на первый день
        INSERT INTO attendance (student_id, date_attended, status) VALUES
        (student_rec.student_id, '2024-01-10', 
         CASE WHEN RANDOM() > 0.2 THEN 'присутствовал' ELSE 'отсутствовал' END);
        
        -- Посещаемость на второй день
        INSERT INTO attendance (student_id, date_attended, status) VALUES
        (student_rec.student_id, '2024-01-11',
         CASE WHEN RANDOM() > 0.3 THEN 'присутствовал' ELSE 'отсутствовал' END);
    END LOOP;
END $$;


INSERT INTO notes (student_id, note_text) VALUES
(1, 'Любит информатику, всегда активен на занятиях.'),
(2, 'Нужна помощь по информатике, не понимает базовые алгоритмы.'),
(3, 'Отличник по всем предметам, особенно преуспевает в математическом анализе.'),
(4, 'Хорошо работает в команде, лидер в групповых проектах.'),
(5, 'Редко посещает занятия по информатике, но успевает сдавать работы.'),
(6, 'Прогресс по информатике заметен, начал показывать хорошие результаты.');


CREATE INDEX idx_students_group ON students(group_number);
CREATE INDEX idx_grades_student ON grades(student_id);
CREATE INDEX idx_grades_student_subject ON grades(student_id, subject_id);
CREATE INDEX idx_attendance_student_date ON attendance(student_id, date_attended);

ALTER TABLE notes ADD COLUMN note_tsvector TSVECTOR;
UPDATE notes SET note_tsvector = to_tsvector('russian', note_text);
CREATE INDEX idx_notes_text_gin ON notes USING GIN(note_tsvector);

CREATE TRIGGER notes_tsvector_update_trigger
BEFORE INSERT OR UPDATE ON notes
FOR EACH ROW EXECUTE FUNCTION notes_tsvector_update();

CREATE VIEW student_avg_grades AS
SELECT 
    s.student_id,
    s.full_name,
    s.group_number,
    ROUND(AVG(g.grade)::NUMERIC, 2) as average_grade,
    COUNT(g.grade_id) as total_grades
FROM students s
LEFT JOIN grades g ON s.student_id = g.student_id
GROUP BY s.student_id, s.full_name, s.group_number
ORDER BY average_grade DESC NULLS LAST;


CREATE VIEW student_full_performance AS
SELECT 
    s.full_name,
    s.group_number,
    sub.subject_name,
    ROUND(AVG(g.grade)::NUMERIC, 2) as subject_avg_grade,
    COUNT(g.grade_id) as grades_count
FROM students s
JOIN grades g ON s.student_id = g.student_id
JOIN subjects sub ON g.subject_id = sub.subject_id
GROUP BY s.student_id, s.full_name, s.group_number, sub.subject_name
ORDER BY s.full_name, sub.subject_name;

BEGIN;

INSERT INTO students (full_name, group_number) 
VALUES ('Новиков Сергей Александрович', 'ИВТ-101')
RETURNING student_id INTO new_student_id;

DECLARE new_student_id INTEGER;
INSERT INTO grades (student_id, subject_id, grade) 
SELECT new_student_id, subject_id, (RANDOM() * 2 + 3)::INTEGER 
FROM subjects;

INSERT INTO attendance (student_id, date_attended, status) VALUES
(new_student_id, CURRENT_DATE, 'присутствовал');

INSERT INTO notes (student_id, note_text) VALUES
(new_student_id, 'Новый студент, показывающий интерес к информатике.');

COMMIT;

-- Запрос 1
WITH student_info AS (
    SELECT student_id, ROW_NUMBER() OVER (ORDER BY student_id) as row_num
    FROM students 
    WHERE group_number = (SELECT group_number FROM students WHERE student_id = 3)
),
target_student AS (
    SELECT row_num FROM student_info WHERE student_id = 3
)
SELECT s.* 
FROM students s
JOIN student_info si ON s.student_id = si.student_id
WHERE si.row_num BETWEEN 
    (SELECT row_num FROM target_student) - 2 
    AND (SELECT row_num FROM target_student) + 3
AND s.student_id != 3
ORDER BY si.row_num
LIMIT 5;

-- Запрос 2
SELECT * FROM student_avg_grades 
WHERE student_id = 1;

-- Запрос 3
SELECT 
    s.subject_name,
    ROUND(AVG(g.grade)::NUMERIC, 2) as average_grade,
    COUNT(g.grade_id) as total_grades,
    MIN(g.grade) as min_grade,
    MAX(g.grade) as max_grade
FROM grades g
JOIN subjects s ON g.subject_id = s.subject_id
WHERE s.subject_name = 'Информатика'
GROUP BY s.subject_id, s.subject_name;

-- Запрос 4
SELECT 
    n.note_id,
    s.full_name,
    s.group_number,
    n.note_text,
    ts_rank_cd(n.note_tsvector, query) as rank
FROM notes n
JOIN students s ON n.student_id = s.student_id
CROSS JOIN to_tsquery('russian', 'Информатика') query
WHERE n.note_tsvector @@ query
ORDER BY rank DESC;

-- Запрос 5
BEGIN;

INSERT INTO attendance (student_id, date_attended, status) 
VALUES (1, '2024-01-15', 'опоздал')
ON CONFLICT (student_id, date_attended) 
DO UPDATE SET status = EXCLUDED.status;

COMMIT;
