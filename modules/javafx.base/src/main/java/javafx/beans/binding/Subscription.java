package javafx.beans.binding;

import java.util.Objects;
import java.util.function.Consumer;

import javafx.beans.InvalidationListener;
import javafx.beans.value.ChangeListener;
import javafx.beans.value.ObservableValue;

/**
 * A subscription encapsulates how to cancel it without having
 * to keep track of how it was created.<p>
 *
 * For example:<p>
 * <pre>Subscription s = property.subscribe(System.out::println)</pre>
 * The function passed in to {@code subscribe} does not need to be stored
 * in order to clean up the subscription later.
 */
@FunctionalInterface
interface Subscription {

    /**
     * An empty subscription. Does nothing when cancelled.
     */
    static final Subscription EMPTY = () -> {};

    /**
     * Cancels this subscription.
     */
    void unsubscribe();

    /**
     * Combines this {@link Subscription} with the given {@code Subscription}
     * and returns a new {@code Subscription} which will cancel both when
     * cancelled.
     *
     * @param other another {@link Subscription}, cannot be null
     * @return a combined {@link Subscription} which will cancel both when
     *   cancelled, never null
     */
    default Subscription and(Subscription other) {
        Objects.requireNonNull(other);

        return () -> {
            unsubscribe();
            other.unsubscribe();
        };
    }

    /**
     * Creates a {@link Subscription} on this {@link ObservableValue} which
     * immediately provides its current value to the given {@code subscriber},
     * followed by any subsequent changes in value.
     *
     * @param subscriber a {@link Consumer} to supply with the values of this
     *     {@link ObservableValue}, cannot be null
     * @return a {@link Subscription} which can be used to cancel this
     *     subscription, never null
     */
    static <T> Subscription subscribe(ObservableValue<T> observableValue, Consumer<? super T> subscriber) {
        ChangeListener<T> listener = (obs, old, current) -> subscriber.accept(current);

        subscriber.accept(observableValue.getValue());  // eagerly send current value
        observableValue.addListener(listener);

        return () -> observableValue.removeListener(listener);
    }

    /**
     * Creates a {@link Subscription} on this {@link ObservableValue} which
     * calls the given {@code runnable} whenever this {@code ObservableValue}
     * becomes invalid.
     *
     * @param runnable a {@link Runnable} to call whenever this
     *     {@link ObservableValue} becomes invalid, cannot be null
     * @return a {@link Subscription} which can be used to cancel this
     *     subscription, never null
     */
    static Subscription subscribeInvalidations(ObservableValue<?> observableValue, Runnable runnable) {
        InvalidationListener listener = obs -> runnable.run();

        observableValue.addListener(listener);

        return () -> observableValue.removeListener(listener);
    }
}
