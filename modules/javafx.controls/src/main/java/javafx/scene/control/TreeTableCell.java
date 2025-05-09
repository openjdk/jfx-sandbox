/*
 * Copyright (c) 2010, 2024, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

package javafx.scene.control;

import javafx.css.PseudoClass;
import javafx.scene.control.skin.TreeTableCellSkin;
import javafx.beans.InvalidationListener;
import javafx.beans.WeakInvalidationListener;
import javafx.beans.value.ObservableValue;
import javafx.collections.ListChangeListener;
import javafx.event.Event;

import javafx.collections.WeakListChangeListener;
import java.lang.ref.WeakReference;
import javafx.beans.property.ReadOnlyObjectProperty;
import javafx.beans.property.ReadOnlyObjectWrapper;

import javafx.scene.AccessibleAction;
import javafx.scene.AccessibleAttribute;
import javafx.scene.AccessibleRole;
import javafx.scene.control.TreeTableColumn.CellEditEvent;
import javafx.scene.control.TreeTableView.TreeTableViewFocusModel;


/**
 * Represents a single row/column intersection in a {@link TreeTableView}. To
 * represent this intersection, a TreeTableCell contains an
 * {@link #indexProperty() index} property, as well as a
 * {@link #tableColumnProperty() tableColumn} property. In addition, a TreeTableCell
 * instance knows what {@link TreeTableRow} it exists in.
 *
 * <p><strong>A note about selection:</strong> A TreeTableCell visually shows it is
 * selected when two conditions are met:
 * <ol>
 *   <li>The {@link TableSelectionModel#isSelected(int, TableColumnBase)} method
 *   returns true for the row / column that this cell represents, and</li>
 *   <li>The {@link javafx.scene.control.TableSelectionModel#cellSelectionEnabledProperty() cell selection mode}
 *   property is set to true (to represent that it is allowable to select
 *   individual cells (and not just rows of cells)).</li>
 * </ol>
 *
 * @param <S> The type of the TreeTableView generic type
 * @see TreeTableView
 * @see TreeTableColumn
 * @see Cell
 * @see IndexedCell
 * @see TreeTableRow
 * @param <T> The type of the item contained within the Cell.
 * @since JavaFX 8.0
 */
public class TreeTableCell<S,T> extends IndexedCell<T> {

    /* *************************************************************************
     *                                                                         *
     * Constructors                                                            *
     *                                                                         *
     **************************************************************************/

    /**
     * Constructs a default TreeTableCell instance with a style class of
     * 'tree-table-cell'.
     */
    public TreeTableCell() {
        getStyleClass().addAll(DEFAULT_STYLE_CLASS);
        setAccessibleRole(AccessibleRole.TREE_TABLE_CELL);

        updateColumnIndex();
    }



    /* *************************************************************************
     *                                                                         *
     * Private fields                                                          *
     *                                                                         *
     **************************************************************************/




    /* *************************************************************************
     *                                                                         *
     * Callbacks and Events                                                    *
     *                                                                         *
     **************************************************************************/

    private boolean itemDirty = false;

    /*
     * This is the list observer we use to keep an eye on the SelectedCells
     * ObservableList in the tree table view. Because it is possible that the table can
     * be mutated, we create this observer here, and add/remove it from the
     * storeTableView method.
     */
    private ListChangeListener<TreeTablePosition<S,?>> selectedListener = c -> {
        while (c.next()) {
            if (c.wasAdded() || c.wasRemoved()) {
                updateSelection();
            }
        }
    };

    // same as above, but for focus
    private final InvalidationListener focusedListener = value -> {
        updateFocus();
    };

    // same as above, but for for changes to the properties on TableRow
    private final InvalidationListener tableRowUpdateObserver = value -> {
        itemDirty = true;
        requestLayout();
    };

    private final InvalidationListener editingListener = value -> {
        updateEditing();
    };

    private ListChangeListener<TreeTableColumn<S,?>> visibleLeafColumnsListener = c -> {
        updateColumnIndex();
    };

    private ListChangeListener<String> columnStyleClassListener = c -> {
        while (c.next()) {
            if (c.wasRemoved()) {
                getStyleClass().removeAll(c.getRemoved());
            }

            if (c.wasAdded()) {
                getStyleClass().addAll(c.getAddedSubList());
            }
        }
    };

    private final InvalidationListener rootPropertyListener = observable -> {
        updateItem(-1);
    };

    private final InvalidationListener columnStyleListener = value -> {
        if (getTableColumn() != null) {
            possiblySetStyle(getTableColumn().getStyle());
        }
    };

    private final InvalidationListener columnIdListener = value -> {
        if (getTableColumn() != null) {
            possiblySetId(getTableColumn().getId());
        }
    };

    private final WeakListChangeListener<TreeTablePosition<S,?>> weakSelectedListener =
            new WeakListChangeListener<>(selectedListener);
    private final WeakInvalidationListener weakFocusedListener =
            new WeakInvalidationListener(focusedListener);
    private final WeakInvalidationListener weaktableRowUpdateObserver =
            new WeakInvalidationListener(tableRowUpdateObserver);
    private final WeakInvalidationListener weakEditingListener =
            new WeakInvalidationListener(editingListener);
    private final WeakListChangeListener<TreeTableColumn<S,?>> weakVisibleLeafColumnsListener =
            new WeakListChangeListener<>(visibleLeafColumnsListener);
    private final WeakListChangeListener<String> weakColumnStyleClassListener =
            new WeakListChangeListener<>(columnStyleClassListener);
    private final WeakInvalidationListener weakColumnStyleListener =
            new WeakInvalidationListener(columnStyleListener);
    private final WeakInvalidationListener weakColumnIdListener =
            new WeakInvalidationListener(columnIdListener);
    private final WeakInvalidationListener weakRootPropertyListener =
            new WeakInvalidationListener(rootPropertyListener);


    /* *************************************************************************
     *                                                                         *
     * Properties                                                              *
     *                                                                         *
     **************************************************************************/

    // --- TableColumn
    /**
     * The {@code TreeTableColumn} instance that backs this {@code TreeTableCell}.
     */
    private ReadOnlyObjectWrapper<TreeTableColumn<S,T>> tableColumn =
            new ReadOnlyObjectWrapper<>(this, "tableColumn") {
        @Override protected void invalidated() {
            updateColumnIndex();
        }
    };
    public final ReadOnlyObjectProperty<TreeTableColumn<S,T>> tableColumnProperty() { return tableColumn.getReadOnlyProperty(); }
    private void setTableColumn(TreeTableColumn<S,T> value) { tableColumn.set(value); }
    public final TreeTableColumn<S,T> getTableColumn() { return tableColumn.get(); }


    // --- TableView
    /**
     * The {@code TreeTableView} associated with this {@code TreeTableCell}.
     */
    private ReadOnlyObjectWrapper<TreeTableView<S>> treeTableView;
    private void setTreeTableView(TreeTableView<S> value) {
        treeTableViewPropertyImpl().set(value);
    }
    public final TreeTableView<S> getTreeTableView() {
        return treeTableView == null ? null : treeTableView.get();
    }
    public final ReadOnlyObjectProperty<TreeTableView<S>> treeTableViewProperty() {
        return treeTableViewPropertyImpl().getReadOnlyProperty();
    }

    private ReadOnlyObjectWrapper<TreeTableView<S>> treeTableViewPropertyImpl() {
        if (treeTableView == null) {
            treeTableView = new ReadOnlyObjectWrapper<>(this, "treeTableView") {
                private WeakReference<TreeTableView<S>> weakTableViewRef;
                @Override protected void invalidated() {
                    TreeTableView.TreeTableViewSelectionModel<S> sm;
                    TreeTableView.TreeTableViewFocusModel<S> fm;

                    if (weakTableViewRef != null) {
                        TreeTableView<S> oldTableView = weakTableViewRef.get();
                        if (oldTableView != null) {
                            sm = oldTableView.getSelectionModel();
                            if (sm != null) {
                                sm.getSelectedCells().removeListener(weakSelectedListener);
                            }

                            fm = oldTableView.getFocusModel();
                            if (fm != null) {
                                fm.focusedCellProperty().removeListener(weakFocusedListener);
                            }

                            oldTableView.editingCellProperty().removeListener(weakEditingListener);
                            oldTableView.getVisibleLeafColumns().removeListener(weakVisibleLeafColumnsListener);
                            oldTableView.rootProperty().removeListener(weakRootPropertyListener);
                        }
                    }

                    TreeTableView<S> newTreeTableView = get();
                    if (newTreeTableView != null) {
                        sm = newTreeTableView.getSelectionModel();
                        if (sm != null) {
                            sm.getSelectedCells().addListener(weakSelectedListener);
                        }

                        fm = newTreeTableView.getFocusModel();
                        if (fm != null) {
                            fm.focusedCellProperty().addListener(weakFocusedListener);
                        }

                        newTreeTableView.editingCellProperty().addListener(weakEditingListener);
                        newTreeTableView.getVisibleLeafColumns().addListener(weakVisibleLeafColumnsListener);
                        newTreeTableView.rootProperty().addListener(weakRootPropertyListener);

                        weakTableViewRef = new WeakReference<>(newTreeTableView);
                    }

                    updateColumnIndex();
                }
            };
        }
        return treeTableView;
    }


    // --- TableRow
    /**
     * The {@code TreeTableRow} that this {@code TreeTableCell} currently finds itself placed within.
     */
    private ReadOnlyObjectWrapper<TreeTableRow<S>> tableRow =
            new ReadOnlyObjectWrapper<>(this, "tableRow");
    private void setTableRow(TreeTableRow<S> value) { tableRow.set(value); }
    /**
     * Gets the value of the property {@code tableRow}.
     * @return the value of the property {@code tableRow}
     * @since 17
     */
    public final TreeTableRow<S> getTableRow() { return tableRow.get(); }
    public final ReadOnlyObjectProperty<TreeTableRow<S>> tableRowProperty() {
        return tableRow.getReadOnlyProperty();
    }

    // The following method was misnamed and is deprecated in favor of the
    // correctly named method.
    /**
     * @deprecated Use {@link getTableRow} instead.
     * @return the {@code TreeTableRow}
     */
    @Deprecated(since = "17")
    public final TreeTableRow<S> getTreeTableRow() { return getTableRow(); }


    /* *************************************************************************
     *                                                                         *
     * Editing API                                                             *
     *                                                                         *
     **************************************************************************/

    // editing location at start of edit - fix for JDK-8187229
    private TreeTablePosition<S, T> editingCellAtStartEdit = null;
    // test-only
    TreeTablePosition<S, T> getEditingCellAtStartEdit() {
        return editingCellAtStartEdit;
    }

    /** {@inheritDoc} */
    @Override public void startEdit() {
        if (isEditing()) return;

        final TreeTableView<S> table = getTreeTableView();
        final TreeTableColumn<S,T> column = getTableColumn();
        final TreeTableRow<S> row = getTableRow();
        if (!isEditable() ||
                (table != null && !table.isEditable()) ||
                (column != null && !column.isEditable()) ||
                (row != null && !row.isEditable())) {
            return;
        }

        updateItem(-1);

        // it makes sense to get the cell into its editing state before firing
        // the event to listeners below, so that's what we're doing here
        // by calling super.startEdit().
        super.startEdit();

        if (!isEditing()) return;

        editingCellAtStartEdit = new TreeTablePosition<>(table, getIndex(), column);
        if (column != null) {
            CellEditEvent<S, T> editEvent = new CellEditEvent<>(
                table,
                editingCellAtStartEdit,
                TreeTableColumn.<S,T>editStartEvent(),
                null
            );

            Event.fireEvent(column, editEvent);
        }
        if (table != null) {
            table.edit(editingCellAtStartEdit.getRow(), editingCellAtStartEdit.getTableColumn());
        }
    }

    /** {@inheritDoc} */
    @Override public void commitEdit(T newValue) {
        if (!isEditing()) return;

        // inform parent classes of the commit, so that they can switch us
        // out of the editing state.
        // This MUST come before the updateItem call below, otherwise it will
        // call cancelEdit(), resulting in both commit and cancel events being
        // fired (as identified in JDK-8124615)
        super.commitEdit(newValue);

        final TreeTableView<S> table = getTreeTableView();
        boolean tableShouldRequestFocus = false;

        if (table != null) {
            // The cell is going to be updated, and the current focus owner might be removed from it.
            // Before that happens, check if it has the table as a parent (otherwise the user might have
            // clicked out of the table entirely and given focus to something else), so the table can
            // request the focus back, once the edit commit ends.
            tableShouldRequestFocus = ControlUtils.controlShouldRequestFocusIfCurrentFocusOwnerIsChild(table);
        }

        // JDK-8187307: fire the commit after updating cell's editing state
        if (getTableColumn() != null) {
            // Inform the TreeTableColumn of the edit being ready to be committed.
            CellEditEvent<S,T> editEvent = new CellEditEvent<>(
                    table,
                    editingCellAtStartEdit,
                    TreeTableColumn.<S,T>editCommitEvent(),
                    newValue
                    );

            Event.fireEvent(getTableColumn(), editEvent);
        }

        // Update the item within this cell, so that it represents the new value
        updateItem(-1);

        if (table != null) {
            // reset the editing cell on the TableView
            table.edit(-1, null);

            // request focus back onto the table, only if the current focus
            // owner had the table as a parent.
            // It would be rude of us to request it back again.
            if (tableShouldRequestFocus) {
                table.requestFocus();
            }
        }
    }

    /** {@inheritDoc} */
    @Override public void cancelEdit() {
        if (!isEditing()) return;

        super.cancelEdit();

        final TreeTableView<S> table = getTreeTableView();
        if (table != null) {
            // reset the editing index on the TableView
            if (updateEditingIndex) table.edit(-1, null);
            // request focus back onto the table, only if the current focus
            // owner has the table as a parent (otherwise the user might have
            // clicked out of the table entirely and given focus to something else).
            // It would be rude of us to request it back again.
            ControlUtils.requestFocusOnControlOnlyIfCurrentFocusOwnerIsChild(table);
        }

        if (getTableColumn() != null) {
            CellEditEvent<S,T> editEvent = new CellEditEvent<>(
                    table,
                    editingCellAtStartEdit,
                    TreeTableColumn.<S,T>editCancelEvent(),
                    null
                    );

            Event.fireEvent(getTableColumn(), editEvent);
        }
    }



    /* *************************************************************************
     *                                                                         *
     * Overriding methods                                                      *
     *                                                                         *
     **************************************************************************/

    /** {@inheritDoc} */
    @Override public void updateSelected(boolean selected) {
        // copied from Cell, with the first conditional clause below commented
        // out, as it is valid for an empty TableCell to be selected, as long
        // as the parent TableRow is not empty (see JDK-8113895).
        /*if (selected && isEmpty()) return;*/
        if (getTableRow() == null || getTableRow().isEmpty()) return;
        setSelected(selected);
    }



    /* *************************************************************************
     *                                                                         *
     * Private Implementation                                                  *
     *                                                                         *
     **************************************************************************/

    /** {@inheritDoc} */
    @Override void indexChanged(int oldIndex, int newIndex) {
        super.indexChanged(oldIndex, newIndex);

        if (isEditing() && newIndex == oldIndex) {
            // no-op
            // Fix for JDK-8123482 - if we (needlessly) update the index whilst the
            // cell is being edited it will no longer be in an editing state.
            // This means that in certain (common) circumstances that it will
            // appear that a cell is uneditable as, despite being clicked, it
            // will not change to the editing state as a layout of VirtualFlow
            // is immediately invoked, which forces all cells to be updated.
        } else {
            // Ideally we would just use the following two lines of code, rather
            // than the updateItem() call beneath, but if we do this we end up with
            // JDK-8126803 where all the columns are collapsed.
            // itemDirty = true;
            // requestLayout();
            updateItem(oldIndex);
            updateSelection();
            updateFocus();
            updateEditing();
        }
    }

    private boolean isLastVisibleColumn = false;
    private int columnIndex = -1;

    private void updateColumnIndex() {
        final TreeTableView<S> tv = getTreeTableView();
        TreeTableColumn<S,T> tc = getTableColumn();
        columnIndex = tv == null || tc == null ? -1 : tv.getVisibleLeafIndex(tc);

        // update the pseudo class state regarding whether this is the last
        // visible cell (i.e. the right-most).
        isLastVisibleColumn = getTableColumn() != null &&
                columnIndex != -1 &&
                columnIndex == tv.getVisibleLeafColumns().size() - 1;
        pseudoClassStateChanged(PSEUDO_CLASS_LAST_VISIBLE, isLastVisibleColumn);
    }

    private void updateSelection() {
        /*
         * This cell should be selected if the selection mode of the table
         * is cell-based, and if the row and column that this cell represents
         * is selected.
         *
         * If the selection mode is not cell-based, then the listener in the
         * TableRow class might pick up the need to set an entire row to be
         * selected.
         */
        if (isEmpty()) return;

        final boolean isSelected = isSelected();
        if (! isInCellSelectionMode()) {
            if (isSelected) {
                updateSelected(false);
            }
            return;
        }

        final TreeTableView<S> tv = getTreeTableView();
        if (getIndex() == -1 || tv == null) return;

        TreeTableView.TreeTableViewSelectionModel<S> sm = tv.getSelectionModel();
        if (sm == null) {
            updateSelected(false);
            return;
        }

        boolean isSelectedNow = sm.isSelected(getIndex(), getTableColumn());
        if (isSelected == isSelectedNow) return;

        updateSelected(isSelectedNow);
    }

    private void updateFocus() {
        final boolean isFocused = isFocused();
        if (! isInCellSelectionMode()) {
            if (isFocused) {
                setFocused(false);
            }
            return;
        }

        final TreeTableView<S> tv = getTreeTableView();
        if (getIndex() == -1 || tv == null) return;

        TreeTableView.TreeTableViewFocusModel<S> fm = tv.getFocusModel();
        if (fm == null) {
            setFocused(false);
            return;
        }

        setFocused(fm.isFocused(getIndex(), getTableColumn()));
    }

    private void updateEditing() {
        final TreeTableView<S> tv = getTreeTableView();
        if (getIndex() == -1 || tv == null) {
            // JDK-8265206: must cancel edit if index changed to -1 by re-use
            if (isEditing()) {
                doCancelEdit();
            }
            return;
        }

        TreeTablePosition<S,?> editCell = tv.getEditingCell();
        boolean match = match(editCell);

        if (match && ! isEditing()) {
            startEdit();
        } else if (! match && isEditing()) {
            doCancelEdit();
        }
    }

    /**
     * Switches an editing cell into not editing without changing control's
     * editing state.
     */
    private void doCancelEdit() {
        // If my index is not the one being edited then I need to cancel
        // the edit. The tricky thing here is that as part of this call
        // I cannot end up calling list.edit(-1) the way that the standard
        // cancelEdit method would do. Yet, I need to call cancelEdit
        // so that subclasses which override cancelEdit can execute. So,
        // I have to use a kind of hacky flag workaround.
        try {
            // try-finally to make certain that the flag is reliably reset to true
            updateEditingIndex = false;
            cancelEdit();
        } finally {
            updateEditingIndex = true;
        }
    }

    private boolean updateEditingIndex = true;

    private boolean match(TreeTablePosition pos) {
        return pos != null && pos.getRow() == getIndex() && pos.getTableColumn() == getTableColumn();
    }

    private boolean isInCellSelectionMode() {
        TreeTableView<S> tv = getTreeTableView();
        if (tv == null) return false;
        TreeTableView.TreeTableViewSelectionModel<S> sm = tv.getSelectionModel();
        return sm != null && sm.isCellSelectionEnabled();
    }

    /*
     * This was brought in to fix the issue in JDK-8126798, namely that the
     * ObservableValue was being GC'd, meaning that changes to the value were
     * no longer being delivered. By extracting this value out of the method,
     * it is now referred to from TableCell and will therefore no longer be
     * GC'd.
     */
    private ObservableValue<T> currentObservableValue = null;

    private boolean isFirstRun = true;

    private WeakReference<S> oldRowItemRef;

    /*
     * This is called when we think that the data within this TreeTableCell may have
     * changed. You'll note that this is a private function - it is only called
     * when one of the triggers above call it.
     */
    private void updateItem(int oldIndex) {
        if (currentObservableValue != null) {
            currentObservableValue.removeListener(weaktableRowUpdateObserver);
        }

        // get the total number of items in the data model
        final TreeTableView<S> tableView = getTreeTableView();
        final TreeTableColumn<S,T> tableColumn = getTableColumn();
        final int itemCount = tableView == null ? -1 : getTreeTableView().getExpandedItemCount();
        final int index = getIndex();
        final boolean isEmpty = isEmpty();
        final T oldValue = getItem();

        final TreeTableRow<S> tableRow = getTableRow();
        final S rowItem = tableRow == null ? null : tableRow.getItem();

        final boolean indexExceedsItemCount = index >= itemCount;

        // there is a whole heap of reasons why we should just punt...
        outer: if (indexExceedsItemCount ||
                index < 0 ||
                columnIndex < 0 ||
                !isVisible() ||
                tableColumn == null ||
                !tableColumn.isVisible() ||
                tableView.getRoot() == null) {

            // JDK-8116529 We need to allow a first run to be special-cased to allow
            // for the updateItem method to be called at least once to allow for
            // the correct visual state to be set up. In particular, in JDK-8116529
            // refer to Ensemble8PopUpTree.png - in this case the arrows are being
            // shown as the new cells are instantiated with the arrows in the
            // children list, and are only hidden in updateItem.
            // JDK-8115233: There are circumstances where we need to updateItem,
            // even when the index is greater than the itemCount. For example,
            // JDK-8115233 identifies issues where a TreeTableView collapses a
            // TreeItem but the custom cells remain visible. This is now
            // resolved with the check for indexExceedsItemCount.
            if ((!isEmpty && oldValue != null) || isFirstRun || indexExceedsItemCount) {
                updateItem(null, true);
                isFirstRun = false;
            }
            return;
        } else {
            currentObservableValue = tableColumn.getCellObservableValue(index);

            final T newValue = currentObservableValue == null ? null : currentObservableValue.getValue();

            // JDK-8092593 - if the index didn't change, then avoid calling updateItem
            // unless the item has changed.
            if (oldIndex == index) {
                if (!isItemChanged(oldValue, newValue)) {
                    // JDK-8096643: we need to check the row item here to prevent
                    // the issue where the cell value and index doesn't change,
                    // but the backing row object does.
                    S oldRowItem = oldRowItemRef != null ? oldRowItemRef.get() : null;
                    if (oldRowItem != null && oldRowItem.equals(rowItem)) {
                        // JDK-8096969:  we break out of the if/else code here and
                        // proceed with the code following this, so that we may
                        // still update references, listeners, etc as required.
                        break outer;
                    }
                }
            }
            updateItem(newValue, false);
        }

        oldRowItemRef = new WeakReference<>(rowItem);

        if (currentObservableValue == null) {
            return;
        }

        // add property change listeners to this item
        currentObservableValue.addListener(weaktableRowUpdateObserver);
    }

    @Override protected void layoutChildren() {
        if (itemDirty) {
            updateItem(-1);
            itemDirty = false;
        }
        super.layoutChildren();
    }




    /* *************************************************************************
     *                                                                         *
     *                              Expert API                                 *
     *                                                                         *
     **************************************************************************/

    /**
     * Updates the {@code TreeTableView} associated with this {@code TreeTableCell}.
     * This is typically only done once when the {@code TreeTableCell} is first
     * added to the {@code TreeTableView}.
     * <p>
     * Note: This function is intended to be used by experts, primarily
     *       by those implementing new Skins. It is not common
     *       for developers or designers to access this function directly.
     * @param tv the {@code TreeTableView} associated with this {@code TreeTableCell}
     */
    public final void updateTreeTableView(TreeTableView<S> tv) {
        setTreeTableView(tv);
    }

    /**
     * Updates the {@code TreeTableRow} associated with this {@code TreeTableCell}.
     * <p>
     * Note: This function is intended to be used by experts, primarily
     *       by those implementing new Skins. It is not common
     *       for developers or designers to access this function directly.
     * @param row the {@code TreeTableRow} associated with this {@code TreeTableCell}
     * @since 17
     */
    public final void updateTableRow(TreeTableRow<S> row) {
        this.setTableRow(row);
    }

    /**
     * Updates the {@code TreeTableColumn} associated with this {@code TreeTableCell}.
     * <p>
     * Note: This function is intended to be used by experts, primarily
     *       by those implementing new Skins. It is not common
     *       for developers or designers to access this function directly.
     * @param column the {@code TreeTableColumn} associated with this {@code TreeTableCell}
     * @since 17
     */
    public final void updateTableColumn(TreeTableColumn<S,T> column) {
        // remove style class of existing tree table column, if it is non-null
        TreeTableColumn<S,T> oldCol = getTableColumn();
        if (oldCol != null) {
            oldCol.getStyleClass().removeListener(weakColumnStyleClassListener);
            getStyleClass().removeAll(oldCol.getStyleClass());

            oldCol.idProperty().removeListener(weakColumnIdListener);
            oldCol.styleProperty().removeListener(weakColumnStyleListener);

            String id = getId();
            String style = getStyle();
            if (id != null && id.equals(oldCol.getId())) {
                setId(null);
            }
            if (style != null && style.equals(oldCol.getStyle())) {
                setStyle("");
            }
        }

        setTableColumn(column);

        if (column != null) {
            getStyleClass().addAll(column.getStyleClass());
            column.getStyleClass().addListener(weakColumnStyleClassListener);

            column.idProperty().addListener(weakColumnIdListener);
            column.styleProperty().addListener(weakColumnStyleListener);

            possiblySetId(column.getId());
            possiblySetStyle(column.getStyle());
        }
    }

    // The following methods were misnamed and are deprecated in favor of the
    // correctly named methods.
    /**
     * @deprecated Use {@link updateTableRow} instead.
     * @param row the {@code TreeTableRow}
     */
    @Deprecated(since = "17")
    public final void updateTreeTableRow(TreeTableRow<S> row) {
        updateTableRow(row);
    }

    /**
     * @deprecated Use {@link updateTableColumn} instead.
     * @param column the {@code TreeTableColumn}
     */
    @Deprecated(since = "17")
    public final void updateTreeTableColumn(TreeTableColumn<S,T> column) {
        updateTableColumn(column);
    }


    /* *************************************************************************
     *                                                                         *
     * Stylesheet Handling                                                     *
     *                                                                         *
     **************************************************************************/

    private static final String DEFAULT_STYLE_CLASS = "tree-table-cell";
    private static final PseudoClass PSEUDO_CLASS_LAST_VISIBLE =
            PseudoClass.getPseudoClass("last-visible");

    /** {@inheritDoc} */
    @Override protected Skin<?> createDefaultSkin() {
        return new TreeTableCellSkin<>(this);
    }

    private void possiblySetId(String idCandidate) {
        if (getId() == null || getId().isEmpty()) {
            setId(idCandidate);
        }
    }

    private void possiblySetStyle(String styleCandidate) {
        if (getStyle() == null || getStyle().isEmpty()) {
            setStyle(styleCandidate);
        }
    }


    /* *************************************************************************
     *                                                                         *
     * Accessibility handling                                                  *
     *                                                                         *
     **************************************************************************/

    /** {@inheritDoc} */
    @Override
    public Object queryAccessibleAttribute(AccessibleAttribute attribute, Object... parameters) {
        switch (attribute) {
        case ROW_INDEX:
            return getIndex();
        case COLUMN_INDEX:
            return columnIndex;
        case SELECTED:
            if (isInCellSelectionMode()) {
                return isSelected();
            } else {
                if (getTableRow() == null) {
                    return null;
                } else {
                    return getTableRow().isSelected();
                }
            }
        default:
            return super.queryAccessibleAttribute(attribute, parameters);
        }
    }

    /** {@inheritDoc} */
    @Override
    public void executeAccessibleAction(AccessibleAction action, Object... parameters) {
        switch (action) {
            case REQUEST_FOCUS: {
                TreeTableView<S> treeTableView = getTreeTableView();
                if (treeTableView != null) {
                    TreeTableViewFocusModel<S> fm = treeTableView.getFocusModel();
                    if (fm != null) {
                        fm.focus(getIndex(), getTableColumn());
                    }
                }
                break;
            }
            default: super.executeAccessibleAction(action, parameters);
        }
    }
}
