package javafx.beans.binding;

import javafx.beans.InvalidationListener;
import javafx.beans.value.ChangeListener;

/**
 * Extends {@link ObjectBinding} with the ability to lazily register and eagerly unregister listeners on its
 * dependencies.
 *
 * @param <T> the type of the wrapped {@code Object}
 */
abstract class LazyObjectBinding<T> extends ObjectBinding<T> {

    private Subscription subscription;
    private boolean wasObserved;

    @Override
    public void addListener(ChangeListener<? super T> listener) {
        super.addListener(listener);

        updateSubcriptionAfterAdd();
    }

    @Override
    public void removeListener(ChangeListener<? super T> listener) {
        super.removeListener(listener);

        updateSubcriptionAfterRemove();
    }

    @Override
    public void addListener(InvalidationListener listener) {
        super.addListener(listener);

        updateSubcriptionAfterAdd();
    }

    @Override
    public void removeListener(InvalidationListener listener) {
        super.removeListener(listener);

        updateSubcriptionAfterRemove();
    }

    @Override
    protected boolean allowValidation() {
        return isObserved();
    }

    private void updateSubcriptionAfterAdd() {
        // isObserved must be true
        if (!wasObserved) { // was first observer registered?
            subscription = observeInputs(); // start observing source

            /*
             * Although the act of registering a listener already attempts to make
             * this binding valid, allowValidation won't allow it as the binding is
             * not observed yet. This is because isObserved will not yet return true
             * when the process of registering the listener hasn't completed yet.
             *
             * As the binding must be valid after it becomes observed the first time
             * 'get' is called again.
             */
            get(); // make binding valid as source wasn't tracked until now
            wasObserved = true;
        }
    }

    private void updateSubcriptionAfterRemove() {
        boolean isObserved = isObserved();
        if (wasObserved && !isObserved) { // was last observer unregistered?
            subscription.unsubscribe();
            subscription = null;
            invalidate(); // make binding invalid as source is no longer tracked
            wasObserved = isObserved;
        }
    }

    /**
     * Called when this binding was previously not observed and a new observer was added. Implementors must return a
     * {@link Subscription} which will be cancelled when this binding no longer has any observers.
     *
     * @return a {@link Subscription} which will be cancelled when this binding no longer has any observers, never null
     */
    protected abstract Subscription observeInputs();
}
