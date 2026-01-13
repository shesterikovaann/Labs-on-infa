CREATE TABLE users (
    user_id SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    email VARCHAR(255) UNIQUE NOT NULL,
    role VARCHAR(20) CHECK (role IN ('admin', 'manager', 'customer')) NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    loyalty_level INTEGER DEFAULT 0 CHECK (loyalty_level IN (0, 1))
);

-- Products table
CREATE TABLE products (
    product_id SERIAL PRIMARY KEY,
    name VARCHAR(200) NOT NULL,
    price DECIMAL(10, 2) CHECK (price > 0) NOT NULL,
    stock_quantity INTEGER CHECK (stock_quantity >= 0) NOT NULL
);

-- Orders table
CREATE TABLE orders (
    order_id SERIAL PRIMARY KEY,
    user_id INTEGER REFERENCES users(user_id) ON DELETE CASCADE,
    status VARCHAR(20) CHECK (status IN ('pending', 'completed', 'canceled', 'returned')) DEFAULT 'pending',
    total_price DECIMAL(10, 2) DEFAULT 0.00,
    order_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Order items table
CREATE TABLE order_items (
    order_item_id SERIAL PRIMARY KEY,
    order_id INTEGER REFERENCES orders(order_id) ON DELETE CASCADE,
    product_id INTEGER REFERENCES products(product_id),
    quantity INTEGER CHECK (quantity > 0) NOT NULL,
    price DECIMAL(10, 2) CHECK (price >= 0) NOT NULL
);

-- Order status history table
CREATE TABLE order_status_history (
    history_id SERIAL PRIMARY KEY,
    order_id INTEGER REFERENCES orders(order_id) ON DELETE CASCADE,
    old_status VARCHAR(20),
    new_status VARCHAR(20) NOT NULL,
    changed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    changed_by INTEGER REFERENCES users(user_id)
);

-- Audit log table
CREATE TABLE audit_log (
    log_id SERIAL PRIMARY KEY,
    entity_type VARCHAR(20) CHECK (entity_type IN ('order', 'product', 'user')) NOT NULL,
    entity_id INTEGER NOT NULL,
    operation VARCHAR(20) CHECK (operation IN ('insert', 'update', 'delete')) NOT NULL,
    performed_by INTEGER REFERENCES users(user_id),
    performed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Indexes for query optimization
CREATE INDEX idx_orders_user_id ON orders(user_id);
CREATE INDEX idx_orders_status ON orders(status);
CREATE INDEX idx_order_items_order_id ON order_items(order_id);
CREATE INDEX idx_audit_log_entity ON audit_log(entity_type, entity_id);
CREATE INDEX idx_audit_log_performed_by ON audit_log(performed_by);
CREATE INDEX idx_order_status_history_order_id ON order_status_history(order_id);

-- ========== 2. FUNCTIONS ==========

-- Function 1: getOrderStatus - returns order status
CREATE OR REPLACE FUNCTION getOrderStatus(p_order_id INTEGER)
RETURNS VARCHAR AS $$
DECLARE
    v_status VARCHAR;
BEGIN
    SELECT status INTO v_status
    FROM orders 
    WHERE order_id = p_order_id;
    
    RETURN COALESCE(v_status, 'Order not found');
EXCEPTION
    WHEN OTHERS THEN
        RETURN 'Error: ' || SQLERRM;
END;
$$ LANGUAGE plpgsql;

-- Function 2: getUserOrderCount - number of orders for each user
CREATE OR REPLACE FUNCTION getUserOrderCount()
RETURNS TABLE(user_id INTEGER, user_name VARCHAR, order_count BIGINT) AS $$
BEGIN
    RETURN QUERY
    SELECT u.user_id, u.name, COUNT(o.order_id)::BIGINT
    FROM users u
    LEFT JOIN orders o ON u.user_id = o.user_id
    GROUP BY u.user_id, u.name
    ORDER BY order_count DESC;
END;
$$ LANGUAGE plpgsql;

-- Function 3: getTotalSpentByUser - total amount spent by user
CREATE OR REPLACE FUNCTION getTotalSpentByUser(p_user_id INTEGER)
RETURNS DECIMAL AS $$
DECLARE
    v_total DECIMAL(10,2);
BEGIN
    SELECT COALESCE(SUM(total_price), 0) INTO v_total
    FROM orders
    WHERE user_id = p_user_id
      AND status NOT IN ('canceled', 'returned');
    
    RETURN v_total;
END;
$$ LANGUAGE plpgsql;

-- Function 4: canReturnOrder - check if order can be returned
CREATE OR REPLACE FUNCTION canReturnOrder(p_order_id INTEGER)
RETURNS BOOLEAN AS $$
DECLARE
    v_status VARCHAR;
    v_order_date TIMESTAMP;
    v_days_passed INTEGER;
BEGIN
    SELECT status, order_date INTO v_status, v_order_date
    FROM orders 
    WHERE order_id = p_order_id;
    
    IF v_status = 'completed' THEN
        v_days_passed := EXTRACT(DAY FROM (CURRENT_TIMESTAMP - v_order_date));
        RETURN v_days_passed <= 30;
    END IF;
    
    RETURN FALSE;
END;
$$ LANGUAGE plpgsql;

-- Function 5: getAuditLogByUser - actions of a specific user
CREATE OR REPLACE FUNCTION getAuditLogByUser(p_user_id INTEGER)
RETURNS TABLE(
    log_id INTEGER,
    entity_type VARCHAR,
    entity_id INTEGER,
    operation VARCHAR,
    performed_at TIMESTAMP,
    performer_name VARCHAR
) AS $$
BEGIN
    RETURN QUERY
    SELECT a.log_id, a.entity_type, a.entity_id, a.operation, a.performed_at, u.name
    FROM audit_log a
    JOIN users u ON a.performed_by = u.user_id
    WHERE a.performed_by = p_user_id
    ORDER BY a.performed_at DESC;
END;
$$ LANGUAGE plpgsql;

-- ========== 3. STORED PROCEDURES ==========

-- Procedure 1: createOrder - create order with transaction
CREATE OR REPLACE PROCEDURE createOrder(
    p_user_id INTEGER,
    p_product_id INTEGER,
    p_quantity INTEGER,
    p_total_price DECIMAL
)
LANGUAGE plpgsql
AS $$
DECLARE
    v_order_id INTEGER;
    v_stock INTEGER;
BEGIN
    -- Start transaction
    BEGIN
        -- Check product availability
        SELECT stock_quantity INTO v_stock
        FROM products 
        WHERE product_id = p_product_id
        FOR UPDATE;
        
        IF v_stock < p_quantity THEN
            RAISE EXCEPTION 'Insufficient stock. Available: %', v_stock;
        END IF;
        
        -- Create order
        INSERT INTO orders (user_id, total_price, status)
        VALUES (p_user_id, p_total_price, 'pending')
        RETURNING order_id INTO v_order_id;
        
        -- Add order item
        INSERT INTO order_items (order_id, product_id, quantity, price)
        VALUES (
            v_order_id, 
            p_product_id, 
            p_quantity, 
            (SELECT price FROM products WHERE product_id = p_product_id)
        );
        
        -- Update product quantity
        UPDATE products 
        SET stock_quantity = stock_quantity - p_quantity
        WHERE product_id = p_product_id;
        
        -- Audit operation
        INSERT INTO audit_log (entity_type, entity_id, operation, performed_by)
        VALUES ('order', v_order_id, 'insert', p_user_id);
        
        -- Commit transaction
        COMMIT;
        
    EXCEPTION
        WHEN OTHERS THEN
            ROLLBACK;
            RAISE;
    END;
END;
$$;

-- Procedure 2: updateOrderStatus - update order status
CREATE OR REPLACE PROCEDURE updateOrderStatus(
    p_order_id INTEGER,
    p_new_status VARCHAR,
    p_changed_by INTEGER
)
LANGUAGE plpgsql
AS $$
DECLARE
    v_old_status VARCHAR;
BEGIN
    -- Get current status
    SELECT status INTO v_old_status
    FROM orders 
    WHERE order_id = p_order_id;
    
    IF v_old_status IS NULL THEN
        RAISE EXCEPTION 'Order with ID % not found', p_order_id;
    END IF;
    
    -- Update status
    UPDATE orders 
    SET status = p_new_status
    WHERE order_id = p_order_id;
    
    -- Save history
    INSERT INTO order_status_history (order_id, old_status, new_status, changed_by)
    VALUES (p_order_id, v_old_status, p_new_status, p_changed_by);
    
    -- Audit operation
    INSERT INTO audit_log (entity_type, entity_id, operation, performed_by)
    VALUES ('order', p_order_id, 'update', p_changed_by);
END;
$$;

-- ========== 4. TRIGGERS ==========

-- Trigger 1: Update order_date when status changes
CREATE OR REPLACE FUNCTION update_order_date_on_status_change()
RETURNS TRIGGER AS $$
BEGIN
    IF OLD.status IS DISTINCT FROM NEW.status THEN
        NEW.order_date = CURRENT_TIMESTAMP;
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trigger_update_order_date
    BEFORE UPDATE ON orders
    FOR EACH ROW
    EXECUTE FUNCTION update_order_date_on_status_change();

-- Trigger 2: Update total_price when product price changes
CREATE OR REPLACE FUNCTION update_order_price_on_product_change()
RETURNS TRIGGER AS $$
BEGIN
    IF OLD.price IS DISTINCT FROM NEW.price THEN
        UPDATE orders o
        SET total_price = (
            SELECT SUM(oi.quantity * NEW.price)
            FROM order_items oi
            WHERE oi.order_id = o.order_id
              AND oi.product_id = NEW.product_id
        )
        WHERE order_id IN (
            SELECT DISTINCT order_id
            FROM order_items
            WHERE product_id = NEW.product_id
        );
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trigger_update_order_price
    AFTER UPDATE OF price ON products
    FOR EACH ROW
    EXECUTE FUNCTION update_order_price_on_product_change();

-- Trigger 3: Log status history
CREATE OR REPLACE FUNCTION log_order_status_history()
RETURNS TRIGGER AS $$
BEGIN
    IF OLD.status IS DISTINCT FROM NEW.status THEN
        INSERT INTO order_status_history (order_id, old_status, new_status, changed_by)
        VALUES (NEW.order_id, OLD.status, NEW.status, NEW.user_id);
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trigger_log_status_history
    AFTER UPDATE OF status ON orders
    FOR EACH ROW
    EXECUTE FUNCTION log_order_status_history();

-- Trigger 4: Audit product operations - FIXED
CREATE OR REPLACE FUNCTION audit_product_operations()
RETURNS TRIGGER AS $$
DECLARE
    v_current_user_id INTEGER;
BEGIN
    -- Get current user ID from session
    -- In a real system, there should be logic to get user_id
    -- For tests, use 1 (admin) or NULL
    v_current_user_id := NULL;
    
    -- Try to get user_id from current context
    BEGIN
        -- If there's a user_id parameter in session context
        v_current_user_id := current_setting('app.user_id', true)::INTEGER;
    EXCEPTION
        WHEN OTHERS THEN
            v_current_user_id := NULL;
    END;
    
    IF TG_OP = 'INSERT' THEN
        INSERT INTO audit_log (entity_type, entity_id, operation, performed_by)
        VALUES ('product', NEW.product_id, 'insert', v_current_user_id);
        RETURN NEW;
    ELSIF TG_OP = 'UPDATE' THEN
        INSERT INTO audit_log (entity_type, entity_id, operation, performed_by)
        VALUES ('product', NEW.product_id, 'update', v_current_user_id);
        RETURN NEW;
    ELSIF TG_OP = 'DELETE' THEN
        INSERT INTO audit_log (entity_type, entity_id, operation, performed_by)
        VALUES ('product', OLD.product_id, 'delete', v_current_user_id);
        RETURN OLD;
    END IF;
    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trigger_audit_products
    AFTER INSERT OR UPDATE OR DELETE ON products
    FOR EACH ROW
    EXECUTE FUNCTION audit_product_operations();

-- Trigger 5: Audit order operations - FIXED
CREATE OR REPLACE FUNCTION audit_order_operations()
RETURNS TRIGGER AS $$
BEGIN
    IF TG_OP = 'INSERT' THEN
        INSERT INTO audit_log (entity_type, entity_id, operation, performed_by)
        VALUES ('order', NEW.order_id, 'insert', NEW.user_id);
        RETURN NEW;
    ELSIF TG_OP = 'UPDATE' THEN
        INSERT INTO audit_log (entity_type, entity_id, operation, performed_by)
        VALUES ('order', NEW.order_id, 'update', NEW.user_id);
        RETURN NEW;
    ELSIF TG_OP = 'DELETE' THEN
        INSERT INTO audit_log (entity_type, entity_id, operation, performed_by)
        VALUES ('order', OLD.order_id, 'delete', OLD.user_id);
        RETURN OLD;
    END IF;
    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trigger_audit_orders
    AFTER INSERT OR UPDATE OR DELETE ON orders
    FOR EACH ROW
    EXECUTE FUNCTION audit_order_operations();

-- Trigger 6: Audit user operations - FIXED
CREATE OR REPLACE FUNCTION audit_user_operations()
RETURNS TRIGGER AS $$
BEGIN
    IF TG_OP = 'INSERT' THEN
        INSERT INTO audit_log (entity_type, entity_id, operation, performed_by)
        VALUES ('user', NEW.user_id, 'insert', NEW.user_id);
        RETURN NEW;
    ELSIF TG_OP = 'UPDATE' THEN
        INSERT INTO audit_log (entity_type, entity_id, operation, performed_by)
        VALUES ('user', NEW.user_id, 'update', NEW.user_id);
        RETURN NEW;
    ELSIF TG_OP = 'DELETE' THEN
        INSERT INTO audit_log (entity_type, entity_id, operation, performed_by)
        VALUES ('user', OLD.user_id, 'delete', OLD.user_id);
        RETURN OLD;
    END IF;
    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trigger_audit_users
    AFTER INSERT OR UPDATE OR DELETE ON users
    FOR EACH ROW
    EXECUTE FUNCTION audit_user_operations();

-- ========== 5. CSV REPORT ==========

-- Function for generating CSV report
CREATE OR REPLACE FUNCTION generateOrderAuditReportCSV()
RETURNS TABLE(
    order_id INTEGER,
    customer_name VARCHAR,
    order_status VARCHAR,
    total_price DECIMAL,
    order_date TIMESTAMP,
    status_change_count BIGINT,
    last_audit_operation VARCHAR,
    last_audit_time TIMESTAMP
) AS $$
BEGIN
    RETURN QUERY
    SELECT 
        o.order_id,
        u.name AS customer_name,
        o.status AS order_status,
        o.total_price,
        o.order_date,
        COUNT(osh.history_id)::BIGINT AS status_change_count,
        MAX(al.operation) FILTER (WHERE al.entity_type = 'order') AS last_audit_operation,
        MAX(al.performed_at) FILTER (WHERE al.entity_type = 'order') AS last_audit_time
    FROM orders o
    JOIN users u ON o.user_id = u.user_id
    LEFT JOIN order_status_history osh ON o.order_id = osh.order_id
    LEFT JOIN audit_log al ON o.order_id = al.entity_id AND al.entity_type = 'order'
    GROUP BY o.order_id, u.name, o.status, o.total_price, o.order_date
    ORDER BY o.order_date DESC;
END;
$$ LANGUAGE plpgsql;

-- ========== 6. TEST DATA ==========

-- Insert test users
INSERT INTO users (name, email, role, password_hash, loyalty_level) VALUES
('System Administrator', 'admin@store.com', 'admin', 'hash_admin123', 1),
('Department Manager', 'manager@store.com', 'manager', 'hash_manager123', 1),
('Ivan Buyer', 'customer1@mail.com', 'customer', 'hash_customer123', 0),
('Anna Buyer', 'customer2@mail.com', 'customer', 'hash_customer456', 1);

-- Insert test products
INSERT INTO products (name, price, stock_quantity) VALUES
('Xiaomi Smartphone', 19999.00, 15),
('Lenovo Laptop', 54999.00, 8),
('Sony Headphones', 8999.00, 25),
('Samsung Tablet', 32999.00, 12),
('Logitech Mouse', 2499.00, 50);

-- Insert test orders
INSERT INTO orders (user_id, status, total_price) VALUES
(3, 'completed', 48997.00),
(4, 'pending', 32999.00),
(3, 'canceled', 8999.00);

-- Insert order items
INSERT INTO order_items (order_id, product_id, quantity, price) VALUES
(1, 1, 2, 19999.00),
(1, 3, 1, 8999.00),
(2, 4, 1, 32999.00),
(3, 3, 1, 8999.00);
