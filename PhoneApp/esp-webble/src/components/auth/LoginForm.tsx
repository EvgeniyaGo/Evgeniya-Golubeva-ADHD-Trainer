import { useState, type FormEvent } from "react";
import { useAuth } from "../../auth/useAuth";

type LoginFormProps = {
  onSuccess: () => void;
  onSwitchToSignup: () => void;
};

function authErrorMessage(error: unknown) {
  if (error instanceof Error) {
    if (error.message.includes("auth/invalid-credential")) {
      return "Email or password is incorrect.";
    }

    if (error.message.includes("auth/invalid-email")) {
      return "Enter a valid email address.";
    }

    if (error.message.includes("auth/too-many-requests")) {
      return "Too many attempts. Try again later.";
    }
  }

  return "Could not log in. Please try again.";
}

export function LoginForm({ onSuccess, onSwitchToSignup }: LoginFormProps) {
  const { login } = useAuth();
  const [submitting, setSubmitting] = useState(false);
  const [message, setMessage] = useState("");

  const handleSubmit = async (event: FormEvent<HTMLFormElement>) => {
    event.preventDefault();
    setMessage("");
    setSubmitting(true);

    const formData = new FormData(event.currentTarget);
    const email = String(formData.get("email") ?? "").trim();
    const password = String(formData.get("password") ?? "");

    try {
      await login(email, password);
      onSuccess();
    } catch (error) {
      setMessage(authErrorMessage(error));
    } finally {
      setSubmitting(false);
    }
  };

  return (
    <form className="auth-form" onSubmit={handleSubmit}>
      <div className="auth-heading">
        <h2 id="auth-modal-title">Log in</h2>
        <p>Access your cube profile and saved training sessions.</p>
      </div>

      <label className="auth-field">
        <span>Email</span>
        <input type="email" name="email" autoComplete="email" required />
      </label>

      <label className="auth-field">
        <span>Password</span>
        <input
          type="password"
          name="password"
          autoComplete="current-password"
          required
        />
      </label>

      <button className="auth-primary" type="submit" disabled={submitting}>
        {submitting ? "Logging in..." : "Log in"}
      </button>

      {message && <p className="auth-message">{message}</p>}

      <p className="auth-switch">
        New here?{" "}
        <button type="button" onClick={onSwitchToSignup}>
          Create an account
        </button>
      </p>
    </form>
  );
}
