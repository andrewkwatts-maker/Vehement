/**
 * Nova3D/Vehement2 Content Browser JavaScript
 * Handles content listing, filtering, and management
 */

// ============================================================================
// State
// ============================================================================

var contentState = {
    allContent: [],
    filteredContent: [],
    selectedId: null,
    currentCategory: 'all',
    searchQuery: '',
    sortBy: 'name',
    sortAscending: true,
    gridView: true,
    categories: {}
};

// ============================================================================
// Initialization
// ============================================================================

document.addEventListener('DOMContentLoaded', function() {
    initContentBrowser();
});

function initContentBrowser() {
    // Set up event listeners
    setupEventListeners();

    // Load initial content
    refreshContent();

    // Listen for C++ events
    WebEditor.onMessage('contentRefreshed', function() {
        refreshContent();
    });

    WebEditor.onMessage('itemSelected', function(data) {
        if (data && data.id) {
            selectItem(data.id, false);
        }
    });
}

function setupEventListeners() {
    // Search input
    var searchInput = document.getElementById('search');
    if (searchInput) {
        searchInput.addEventListener('input', debounce(function() {
            contentState.searchQuery = this.value;
            applyFilters();
        }, 200));
    }

    // Sort options
    var sortSelect = document.getElementById('sort-by');
    if (sortSelect) {
        sortSelect.addEventListener('change', function() {
            contentState.sortBy = this.value;
            applyFilters();
        });
    }

    // Keyboard navigation
    document.addEventListener('keydown', function(e) {
        if (e.key === 'Delete' && contentState.selectedId) {
            deleteSelected();
        }
        if (e.key === 'Enter' && contentState.selectedId) {
            editSelected();
        }
        if (e.key === 'Escape') {
            clearSelection();
        }
    });
}

// ============================================================================
// Content Loading
// ============================================================================

function refreshContent() {
    WebEditor.invoke('contentBrowser.getContent', [], function(err, content) {
        if (err) {
            console.error('Failed to load content:', err);
            showEmptyState('Failed to load content');
            return;
        }

        contentState.allContent = content || [];
        updateCategoryCounts();
        applyFilters();
    });

    // Load categories
    WebEditor.invoke('contentBrowser.getCategories', [], function(err, categories) {
        if (!err && categories) {
            categories.forEach(function(cat) {
                contentState.categories[cat.type] = cat.count;
            });
            updateCategoryCounts();
        }
    });
}

function updateCategoryCounts() {
    // Count items per category
    var counts = { all: contentState.allContent.length };

    contentState.allContent.forEach(function(item) {
        var type = item.type;
        counts[type] = (counts[type] || 0) + 1;
    });

    // Update UI
    Object.keys(counts).forEach(function(type) {
        var countEl = document.getElementById('count-' + type);
        if (countEl) {
            countEl.textContent = counts[type];
        }
    });
}

// ============================================================================
// Filtering and Sorting
// ============================================================================

function applyFilters() {
    var content = contentState.allContent.slice();

    // Filter by category
    if (contentState.currentCategory !== 'all') {
        content = content.filter(function(item) {
            return item.type === contentState.currentCategory;
        });
    }

    // Filter by search query
    if (contentState.searchQuery) {
        var query = contentState.searchQuery.toLowerCase();
        content = content.filter(function(item) {
            return (item.name && item.name.toLowerCase().indexOf(query) !== -1) ||
                   (item.description && item.description.toLowerCase().indexOf(query) !== -1) ||
                   (item.id && item.id.toLowerCase().indexOf(query) !== -1);
        });
    }

    // Sort
    content.sort(function(a, b) {
        var aVal, bVal;

        switch (contentState.sortBy) {
            case 'name':
                aVal = (a.name || '').toLowerCase();
                bVal = (b.name || '').toLowerCase();
                break;
            case 'type':
                aVal = a.type || '';
                bVal = b.type || '';
                break;
            case 'modified':
                aVal = a.lastModified || '';
                bVal = b.lastModified || '';
                break;
            default:
                aVal = a.name || '';
                bVal = b.name || '';
        }

        var result = aVal < bVal ? -1 : (aVal > bVal ? 1 : 0);
        return contentState.sortAscending ? result : -result;
    });

    contentState.filteredContent = content;
    renderContent();
}

function selectCategory(type) {
    contentState.currentCategory = type;

    // Update UI
    var categories = document.querySelectorAll('.category');
    categories.forEach(function(cat) {
        removeClass(cat, 'active');
        if (cat.getAttribute('data-type') === type) {
            addClass(cat, 'active');
        }
    });

    // Update header
    var header = document.getElementById('current-category');
    if (header) {
        var names = {
            all: 'All Content',
            spells: 'Spells',
            units: 'Units',
            buildings: 'Buildings',
            techtrees: 'Tech Trees',
            effects: 'Effects',
            buffs: 'Buffs',
            cultures: 'Cultures',
            heroes: 'Heroes'
        };
        header.textContent = names[type] || type;
    }

    applyFilters();
}

function sortContent() {
    var select = document.getElementById('sort-by');
    if (select) {
        contentState.sortBy = select.value;
    }
    applyFilters();
}

function toggleSortOrder() {
    contentState.sortAscending = !contentState.sortAscending;

    var icon = document.getElementById('sort-order-icon');
    if (icon) {
        icon.innerHTML = contentState.sortAscending ? '&#x25B2;' : '&#x25BC;';
    }

    applyFilters();
}

function clearSearch() {
    var searchInput = document.getElementById('search');
    if (searchInput) {
        searchInput.value = '';
    }
    contentState.searchQuery = '';
    applyFilters();
}

// ============================================================================
// Content Rendering
// ============================================================================

function renderContent() {
    var content = contentState.filteredContent;

    if (content.length === 0) {
        showEmptyState('No content found');
        return;
    }

    hideEmptyState();

    if (contentState.gridView) {
        renderGridView(content);
    } else {
        renderListView(content);
    }
}

function renderGridView(content) {
    var grid = document.getElementById('content-grid');
    var list = document.getElementById('content-list');

    if (grid) grid.style.display = 'grid';
    if (list) list.style.display = 'none';

    if (!grid) return;

    grid.innerHTML = '';

    content.forEach(function(item) {
        var el = createElement('div', {
            className: 'content-item' + (item.id === contentState.selectedId ? ' selected' : '') +
                       (item.isDirty ? ' dirty' : ''),
            'data-id': item.id,
            onClick: function(e) {
                selectItem(item.id, e.ctrlKey || e.metaKey);
            },
            onDblclick: function() {
                editItem(item.id);
            },
            onContextmenu: function(e) {
                e.preventDefault();
                selectItem(item.id);
                showItemContextMenu(e.clientX, e.clientY, item);
            }
        }, [
            createElement('div', { className: 'thumbnail' }, getTypeIcon(item.type)),
            createElement('div', { className: 'name' }, item.name || item.id),
            createElement('div', { className: 'type' }, item.type)
        ]);

        // Drag support
        el.draggable = true;
        el.addEventListener('dragstart', function(e) {
            e.dataTransfer.setData('application/json', JSON.stringify(item));
            e.dataTransfer.setData('text/plain', item.id);
        });

        grid.appendChild(el);
    });
}

function renderListView(content) {
    var grid = document.getElementById('content-grid');
    var list = document.getElementById('content-list');

    if (grid) grid.style.display = 'none';
    if (list) list.style.display = 'block';

    if (!list) return;

    list.innerHTML = '';

    content.forEach(function(item) {
        var el = createElement('div', {
            className: 'list-item' + (item.id === contentState.selectedId ? ' selected' : ''),
            'data-id': item.id,
            onClick: function(e) {
                selectItem(item.id, e.ctrlKey || e.metaKey);
            },
            onDblclick: function() {
                editItem(item.id);
            }
        }, [
            createElement('span', { className: 'icon' }, getTypeIcon(item.type)),
            createElement('span', { className: 'name' }, item.name || item.id),
            createElement('span', { className: 'type' }, item.type),
            createElement('span', { className: 'modified' }, item.lastModified || '')
        ]);

        list.appendChild(el);
    });
}

function getTypeIcon(type) {
    var icons = {
        spells: '\u2728',     // Sparkles
        units: '\u2694',      // Swords
        buildings: '\u26F1',  // Umbrella (building-like)
        techtrees: '\u26ED',  // Gear
        effects: '\u2747',    // Sparkle
        buffs: '\u2B50',      // Star
        cultures: '\u26E8',   // Flag
        heroes: '\u26A1'      // Lightning
    };
    return icons[type] || '\u26AB';  // Black circle
}

function toggleGridView() {
    contentState.gridView = !contentState.gridView;

    var icon = document.querySelector('.grid-icon');
    if (icon) {
        icon.innerHTML = contentState.gridView ? '\u2630' : '\u2637';
    }

    renderContent();
}

function showEmptyState(message) {
    var emptyState = document.getElementById('empty-state');
    var grid = document.getElementById('content-grid');
    var list = document.getElementById('content-list');

    if (emptyState) {
        emptyState.style.display = 'flex';
        var msgEl = emptyState.querySelector('h3');
        if (msgEl) msgEl.textContent = message || 'No content found';
    }
    if (grid) grid.style.display = 'none';
    if (list) list.style.display = 'none';
}

function hideEmptyState() {
    var emptyState = document.getElementById('empty-state');
    if (emptyState) {
        emptyState.style.display = 'none';
    }
}

// ============================================================================
// Selection
// ============================================================================

function selectItem(id, addToSelection) {
    contentState.selectedId = id;

    // Update visual selection
    var items = document.querySelectorAll('.content-item, .list-item');
    items.forEach(function(item) {
        if (item.getAttribute('data-id') === id) {
            addClass(item, 'selected');
        } else if (!addToSelection) {
            removeClass(item, 'selected');
        }
    });

    // Notify C++
    WebEditor.invoke('contentBrowser.selectItem', [id]);

    // Update preview
    updatePreview(id);
}

function clearSelection() {
    contentState.selectedId = null;

    var items = document.querySelectorAll('.content-item.selected, .list-item.selected');
    items.forEach(function(item) {
        removeClass(item, 'selected');
    });

    clearPreview();
}

// ============================================================================
// Preview Panel
// ============================================================================

function updatePreview(id) {
    var item = contentState.allContent.find(function(c) { return c.id === id; });
    if (!item) {
        clearPreview();
        return;
    }

    var previewContent = document.getElementById('preview-content');
    var previewActions = document.getElementById('preview-actions');

    if (previewContent) {
        previewContent.innerHTML = '';

        // Thumbnail
        var thumb = createElement('div', { className: 'preview-thumbnail' }, getTypeIcon(item.type));
        previewContent.appendChild(thumb);

        // Info
        var info = createElement('div', { className: 'preview-info' }, [
            createElement('h4', {}, item.name || item.id),
            createElement('p', { className: 'type' }, item.type),
            createElement('p', { className: 'description' }, item.description || 'No description'),
            createElement('p', { className: 'meta' }, 'Modified: ' + (item.lastModified || 'Unknown'))
        ]);
        previewContent.appendChild(info);

        // Tags
        if (item.tags && item.tags.length > 0) {
            var tagsEl = createElement('div', { className: 'tags' });
            item.tags.forEach(function(tag) {
                tagsEl.appendChild(createElement('span', { className: 'tag' }, tag));
            });
            previewContent.appendChild(tagsEl);
        }
    }

    if (previewActions) {
        previewActions.style.display = 'flex';
    }
}

function clearPreview() {
    var previewContent = document.getElementById('preview-content');
    var previewActions = document.getElementById('preview-actions');

    if (previewContent) {
        previewContent.innerHTML = '<p class="no-selection">Select an item to preview</p>';
    }
    if (previewActions) {
        previewActions.style.display = 'none';
    }
}

function togglePreviewPanel() {
    var panel = document.getElementById('preview-panel');
    if (panel) {
        panel.style.display = panel.style.display === 'none' ? 'flex' : 'none';
    }
}

// ============================================================================
// CRUD Operations
// ============================================================================

function createNew() {
    var typeSelect = document.getElementById('new-type');
    var type = typeSelect ? typeSelect.value : '';

    if (!type) {
        alert('Please select a content type');
        return;
    }

    showCreateDialog(type);
}

function showCreateDialog(type) {
    var dialog = document.getElementById('create-dialog');
    if (dialog) {
        dialog.style.display = 'flex';
        dialog.setAttribute('data-type', type);

        var nameInput = document.getElementById('new-name');
        if (nameInput) {
            nameInput.value = '';
            nameInput.focus();
        }
    }
}

function closeCreateDialog() {
    var dialog = document.getElementById('create-dialog');
    if (dialog) {
        dialog.style.display = 'none';
    }
}

function confirmCreate() {
    var dialog = document.getElementById('create-dialog');
    var type = dialog ? dialog.getAttribute('data-type') : '';
    var nameInput = document.getElementById('new-name');
    var name = nameInput ? nameInput.value.trim() : '';

    if (!name) {
        alert('Please enter a name');
        return;
    }

    WebEditor.invoke('contentBrowser.createItem', [type, name], function(err, id) {
        if (err) {
            alert('Failed to create item: ' + err);
            return;
        }

        closeCreateDialog();
        refreshContent();

        // Select and edit new item
        setTimeout(function() {
            selectItem(id);
            editItem(id);
        }, 100);
    });
}

function editSelected() {
    if (contentState.selectedId) {
        editItem(contentState.selectedId);
    }
}

function editItem(id) {
    var item = contentState.allContent.find(function(c) { return c.id === id; });
    if (!item) return;

    // Navigate to appropriate editor
    var editors = {
        spells: 'spell_editor.html',
        units: 'unit_editor.html',
        buildings: 'building_editor.html',
        techtrees: 'techtree_editor.html',
        effects: 'effect_editor.html',
        buffs: 'effect_editor.html'
    };

    var editor = editors[item.type] || 'schema_editor.html';
    WebEditor.navigate(editor, { id: id, type: item.type });
}

function duplicateSelected() {
    if (!contentState.selectedId) return;

    WebEditor.invoke('contentBrowser.duplicateItem', [contentState.selectedId], function(err, newId) {
        if (err) {
            alert('Failed to duplicate: ' + err);
            return;
        }

        refreshContent();
        setTimeout(function() {
            selectItem(newId);
        }, 100);
    });
}

function deleteSelected() {
    if (!contentState.selectedId) return;

    var item = contentState.allContent.find(function(c) { return c.id === contentState.selectedId; });
    var name = item ? item.name : contentState.selectedId;

    if (!confirm('Delete "' + name + '"? This cannot be undone.')) {
        return;
    }

    WebEditor.invoke('contentBrowser.deleteItem', [contentState.selectedId], function(err) {
        if (err) {
            alert('Failed to delete: ' + err);
            return;
        }

        contentState.selectedId = null;
        clearPreview();
        refreshContent();
    });
}

// ============================================================================
// Context Menu
// ============================================================================

function showItemContextMenu(x, y, item) {
    showContextMenu(x, y, [
        { label: 'Edit', onClick: function() { editItem(item.id); } },
        { label: 'Duplicate', onClick: function() {
            WebEditor.invoke('contentBrowser.duplicateItem', [item.id], function(err, newId) {
                if (!err) {
                    refreshContent();
                    setTimeout(function() { selectItem(newId); }, 100);
                }
            });
        }},
        { label: 'Rename', onClick: function() { renameItem(item.id); } },
        { separator: true },
        { label: 'Delete', danger: true, onClick: function() {
            if (confirm('Delete "' + item.name + '"?')) {
                WebEditor.invoke('contentBrowser.deleteItem', [item.id], function(err) {
                    if (!err) {
                        refreshContent();
                        clearPreview();
                    }
                });
            }
        }}
    ], item);
}

function renameItem(id) {
    var item = contentState.allContent.find(function(c) { return c.id === id; });
    if (!item) return;

    var newName = prompt('Enter new name:', item.name);
    if (newName && newName !== item.name) {
        // Save with new name
        WebEditor.invoke('contentBrowser.getItemData', [id], function(err, data) {
            if (err) return;
            data.name = newName;
            WebEditor.invoke('contentBrowser.saveItem', [id, data], function(err) {
                if (!err) {
                    refreshContent();
                }
            });
        });
    }
}

// Export for global use
window.refreshContent = refreshContent;
window.selectCategory = selectCategory;
window.selectItem = selectItem;
window.editItem = editItem;
window.editSelected = editSelected;
window.duplicateSelected = duplicateSelected;
window.deleteSelected = deleteSelected;
window.createNew = createNew;
window.showCreateDialog = showCreateDialog;
window.closeCreateDialog = closeCreateDialog;
window.confirmCreate = confirmCreate;
window.toggleGridView = toggleGridView;
window.togglePreviewPanel = togglePreviewPanel;
window.sortContent = sortContent;
window.toggleSortOrder = toggleSortOrder;
window.clearSearch = clearSearch;
