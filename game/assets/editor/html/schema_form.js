/**
 * Nova3D/Vehement2 Schema Form Generator
 * Auto-generates forms from JSON schema definitions
 */

/**
 * SchemaEditor class - generates and manages form UI from schema
 */
function SchemaEditor(schemaType, options) {
    this.schemaType = schemaType;
    this.options = options || {};
    this.container = typeof this.options.containerSelector === 'string'
        ? document.querySelector(this.options.containerSelector)
        : this.options.containerSelector;

    this.schema = null;
    this.data = {};
    this.originalData = {};
    this.undoStack = [];
    this.redoStack = [];
    this.maxUndoSteps = 50;
    this.validationErrors = {};

    this.onChange = this.options.onDataChange || function() {};
    this.onValidate = this.options.onValidate || function() {};

    this._bindInputEvents();
}

/**
 * Load schema from C++
 */
SchemaEditor.prototype.loadSchema = function(callback) {
    var self = this;
    WebEditor.invoke('schemaEditor.getSchema', [this.schemaType], function(err, schema) {
        if (err) {
            console.error('Failed to load schema:', err);
            return;
        }
        self.schema = schema;
        if (callback) callback(schema);
    });
};

/**
 * Set data for editing
 */
SchemaEditor.prototype.setData = function(data) {
    this.data = deepClone(data);
    this.originalData = deepClone(data);
    this.undoStack = [];
    this.redoStack = [];
    this._populateForm();
    this._validate();
};

/**
 * Get current data
 */
SchemaEditor.prototype.getData = function() {
    this._collectFormData();
    return deepClone(this.data);
};

/**
 * Check if data has changed
 */
SchemaEditor.prototype.isDirty = function() {
    return JSON.stringify(this.data) !== JSON.stringify(this.originalData);
};

/**
 * Mark current data as saved
 */
SchemaEditor.prototype.markClean = function() {
    this.originalData = deepClone(this.data);
};

/**
 * Undo last change
 */
SchemaEditor.prototype.undo = function() {
    if (this.undoStack.length === 0) return false;

    this.redoStack.push(deepClone(this.data));
    this.data = this.undoStack.pop();
    this._populateForm();

    return true;
};

/**
 * Redo last undone change
 */
SchemaEditor.prototype.redo = function() {
    if (this.redoStack.length === 0) return false;

    this.undoStack.push(deepClone(this.data));
    this.data = this.redoStack.pop();
    this._populateForm();

    return true;
};

/**
 * Save state for undo
 */
SchemaEditor.prototype._saveUndoState = function() {
    this.undoStack.push(deepClone(this.data));
    this.redoStack = [];

    if (this.undoStack.length > this.maxUndoSteps) {
        this.undoStack.shift();
    }
};

/**
 * Bind input events for data-path elements
 */
SchemaEditor.prototype._bindInputEvents = function() {
    var self = this;

    if (!this.container) return;

    // Use event delegation
    this.container.addEventListener('change', function(e) {
        var path = e.target.getAttribute('data-path');
        if (path) {
            self._handleInputChange(e.target, path);
        }
    });

    this.container.addEventListener('input', debounce(function(e) {
        var path = e.target.getAttribute('data-path');
        if (path && (e.target.type === 'text' || e.target.tagName === 'TEXTAREA')) {
            self._handleInputChange(e.target, path);
        }
    }, 300));
};

/**
 * Handle input change
 */
SchemaEditor.prototype._handleInputChange = function(input, path) {
    this._saveUndoState();

    var value = this._getInputValue(input);
    setPath(this.data, path, value);

    this._validate();
    this.onChange(path, value);
    setDirty(this.isDirty());
};

/**
 * Get typed value from input
 */
SchemaEditor.prototype._getInputValue = function(input) {
    var type = input.getAttribute('data-type') || input.type;

    switch (type) {
        case 'number':
        case 'integer':
        case 'float':
            var num = parseFloat(input.value);
            return isNaN(num) ? 0 : num;

        case 'checkbox':
        case 'boolean':
            return input.checked;

        case 'select-multiple':
            var values = [];
            for (var i = 0; i < input.options.length; i++) {
                if (input.options[i].selected) {
                    values.push(input.options[i].value);
                }
            }
            return values;

        default:
            return input.value;
    }
};

/**
 * Set value on input
 */
SchemaEditor.prototype._setInputValue = function(input, value) {
    var type = input.getAttribute('data-type') || input.type;

    switch (type) {
        case 'checkbox':
        case 'boolean':
            input.checked = !!value;
            break;

        case 'color':
            input.value = value || '#ffffff';
            break;

        default:
            input.value = value !== undefined && value !== null ? value : '';
    }
};

/**
 * Populate form with current data
 */
SchemaEditor.prototype._populateForm = function() {
    var self = this;

    if (!this.container) return;

    // Find all inputs with data-path
    var inputs = this.container.querySelectorAll('[data-path]');
    inputs.forEach(function(input) {
        var path = input.getAttribute('data-path');
        var value = getPath(self.data, path);
        self._setInputValue(input, value);
    });
};

/**
 * Collect data from form
 */
SchemaEditor.prototype._collectFormData = function() {
    var self = this;

    if (!this.container) return;

    var inputs = this.container.querySelectorAll('[data-path]');
    inputs.forEach(function(input) {
        var path = input.getAttribute('data-path');
        var value = self._getInputValue(input);
        setPath(self.data, path, value);
    });
};

/**
 * Validate data against schema
 */
SchemaEditor.prototype._validate = function() {
    var self = this;
    this.validationErrors = {};

    if (!this.schema || !this.schema.fields) return;

    this.schema.fields.forEach(function(field) {
        var value = getPath(self.data, field.name);
        var error = self._validateField(field, value);
        if (error) {
            self.validationErrors[field.name] = error;
        }
    });

    this._updateErrorDisplay();
    this.onValidate(this.validationErrors);
};

/**
 * Validate a single field
 */
SchemaEditor.prototype._validateField = function(field, value) {
    // Required check
    if (field.required && (value === undefined || value === null || value === '')) {
        return 'This field is required';
    }

    if (value === undefined || value === null) {
        return null;
    }

    var constraints = field.constraints || {};

    // Type checks
    switch (field.type) {
        case 1: // Integer
        case 2: // Float
            if (typeof value !== 'number' || isNaN(value)) {
                return 'Must be a number';
            }
            if (constraints.minValue !== undefined && value < constraints.minValue) {
                return 'Must be at least ' + constraints.minValue;
            }
            if (constraints.maxValue !== undefined && value > constraints.maxValue) {
                return 'Must be at most ' + constraints.maxValue;
            }
            break;

        case 0: // String
            if (typeof value !== 'string') {
                return 'Must be a string';
            }
            if (constraints.minLength !== undefined && value.length < constraints.minLength) {
                return 'Must be at least ' + constraints.minLength + ' characters';
            }
            if (constraints.maxLength !== undefined && value.length > constraints.maxLength) {
                return 'Must be at most ' + constraints.maxLength + ' characters';
            }
            if (constraints.pattern) {
                var regex = new RegExp(constraints.pattern);
                if (!regex.test(value)) {
                    return 'Invalid format';
                }
            }
            break;

        case 8: // Enum
            if (constraints.enumValues && constraints.enumValues.indexOf(value) === -1) {
                return 'Invalid value. Must be one of: ' + constraints.enumValues.join(', ');
            }
            break;
    }

    return null;
};

/**
 * Update error display on inputs
 */
SchemaEditor.prototype._updateErrorDisplay = function() {
    var self = this;

    if (!this.container) return;

    // Clear all errors
    var inputs = this.container.querySelectorAll('[data-path]');
    inputs.forEach(function(input) {
        removeClass(input, 'error');
        var errorSpan = input.parentElement.querySelector('.error-message');
        if (errorSpan) {
            errorSpan.textContent = '';
        }
    });

    // Show errors
    Object.keys(this.validationErrors).forEach(function(path) {
        var input = self.container.querySelector('[data-path="' + path + '"]');
        if (input) {
            addClass(input, 'error');
            var errorSpan = input.parentElement.querySelector('.error-message');
            if (errorSpan) {
                errorSpan.textContent = self.validationErrors[path];
            }
        }
    });
};

/**
 * Get validation errors
 */
SchemaEditor.prototype.getErrors = function() {
    return this.validationErrors;
};

/**
 * Check if form is valid
 */
SchemaEditor.prototype.isValid = function() {
    return Object.keys(this.validationErrors).length === 0;
};

/**
 * Focus on a specific field
 */
SchemaEditor.prototype.focusField = function(path) {
    if (!this.container) return;

    var input = this.container.querySelector('[data-path="' + path + '"]');
    if (input) {
        input.focus();
        input.scrollIntoView({ behavior: 'smooth', block: 'center' });
    }
};

/**
 * Generate form HTML from schema
 */
SchemaEditor.prototype.generateFormHtml = function() {
    if (!this.schema || !this.schema.fields) {
        return '<p>No schema loaded</p>';
    }

    var html = '<form class="schema-form">';

    this.schema.fields.forEach(function(field) {
        html += generateFieldHtml(field, field.name);
    });

    html += '</form>';
    return html;
};

/**
 * Generate HTML for a single field
 */
function generateFieldHtml(field, path) {
    var html = '<div class="form-group">';

    // Label
    html += '<label for="' + path + '">' + field.name;
    if (field.required) {
        html += ' <span class="required">*</span>';
    }
    html += '</label>';

    // Description/help text
    if (field.description) {
        html += '<small class="help-text">' + field.description + '</small>';
    }

    // Input based on type
    var type = field.type;
    var constraints = field.constraints || {};

    switch (type) {
        case 0: // String
            html += '<input type="text" id="' + path + '" data-path="' + path + '"';
            if (constraints.maxLength) html += ' maxlength="' + constraints.maxLength + '"';
            html += '>';
            break;

        case 1: // Integer
            html += '<input type="number" id="' + path + '" data-path="' + path + '" step="1"';
            if (constraints.minValue !== undefined) html += ' min="' + constraints.minValue + '"';
            if (constraints.maxValue !== undefined) html += ' max="' + constraints.maxValue + '"';
            html += '>';
            break;

        case 2: // Float
            html += '<input type="number" id="' + path + '" data-path="' + path + '" step="0.01"';
            if (constraints.minValue !== undefined) html += ' min="' + constraints.minValue + '"';
            if (constraints.maxValue !== undefined) html += ' max="' + constraints.maxValue + '"';
            html += '>';
            break;

        case 3: // Boolean
            html += '<input type="checkbox" id="' + path + '" data-path="' + path + '">';
            break;

        case 8: // Enum
            html += '<select id="' + path + '" data-path="' + path + '">';
            if (constraints.enumValues) {
                constraints.enumValues.forEach(function(val) {
                    html += '<option value="' + val + '">' + val + '</option>';
                });
            }
            html += '</select>';
            break;

        case 6: // Vector3
            html += '<div class="vector3-input">';
            html += '<input type="number" data-path="' + path + '.x" placeholder="X" step="0.1">';
            html += '<input type="number" data-path="' + path + '.y" placeholder="Y" step="0.1">';
            html += '<input type="number" data-path="' + path + '.z" placeholder="Z" step="0.1">';
            html += '</div>';
            break;

        case 7: // Color
            html += '<input type="color" id="' + path + '" data-path="' + path + '">';
            break;

        case 4: // Array
            html += '<div class="array-editor" data-path="' + path + '">';
            html += '<div class="array-items"></div>';
            html += '<button type="button" class="btn btn-small btn-add">+ Add Item</button>';
            html += '</div>';
            break;

        case 5: // Object
            html += '<div class="object-field" data-path="' + path + '">';
            if (field.inlineFields) {
                field.inlineFields.forEach(function(subField) {
                    html += generateFieldHtml(subField, path + '.' + subField.name);
                });
            }
            html += '</div>';
            break;

        default:
            html += '<input type="text" id="' + path + '" data-path="' + path + '">';
    }

    // Error message placeholder
    html += '<span class="error-message"></span>';

    html += '</div>';
    return html;
}

/**
 * Create form from schema definition
 */
function createFormFromSchema(schema, data, onChange) {
    var editor = new SchemaEditor(schema.id, {
        onDataChange: onChange
    });

    editor.schema = schema;
    if (data) {
        editor.setData(data);
    }

    return editor;
}

// Export for global use
window.SchemaEditor = SchemaEditor;
window.createFormFromSchema = createFormFromSchema;
window.generateFieldHtml = generateFieldHtml;
