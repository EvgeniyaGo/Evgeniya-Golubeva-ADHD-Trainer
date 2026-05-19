import { useState, type FormEvent } from "react";
import type { UserRole } from "../../auth/types";
import { useAuth } from "../../auth/useAuth";

type SignupFormProps = {
  onSuccess: () => void;
  onSwitchToLogin: () => void;
};

function authErrorMessage(error: unknown) {
  if (error instanceof Error) {
    if (error.message.includes("auth/email-already-in-use")) {
      return "An account with this email already exists.";
    }

    if (error.message.includes("auth/invalid-email")) {
      return "Enter a valid email address.";
    }

    if (error.message.includes("auth/weak-password")) {
      return "Use a stronger password with at least 6 characters.";
    }
  }

  return "Could not create the account. Please try again.";
}

export function SignupForm({ onSuccess, onSwitchToLogin }: SignupFormProps) {
  const { signup } = useAuth();
  const [role, setRole] = useState<UserRole>("parent");
  const [submitting, setSubmitting] = useState(false);
  const [message, setMessage] = useState("");

  const handleSubmit = async (event: FormEvent<HTMLFormElement>) => {
    event.preventDefault();
    setMessage("");
    setSubmitting(true);

    const formData = new FormData(event.currentTarget);
    const name = String(formData.get("name") ?? "").trim();
    const email = String(formData.get("email") ?? "").trim();
    const password = String(formData.get("password") ?? "");

    try {
      await signup(name, email, password, role);
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
        <h2 id="auth-modal-title">Create account</h2>
        <p>Prepare a profile for tracking cube training progress.</p>
      </div>

      <label className="auth-field">
        <span>Name</span>
        <input type="text" name="name" autoComplete="name" required />
      </label>

      <label className="auth-field">
        <span>Email</span>
        <input type="email" name="email" autoComplete="email" required />
      </label>

      <label className="auth-field">
        <span>Password</span>
        <input
          type="password"
          name="password"
          autoComplete="new-password"
          minLength={6}
          required
        />
      </label>

      <fieldset className="auth-role">
        <legend>Role</legend>
        <label className={role === "parent" ? "is-selected" : ""}>
          <input
            type="radio"
            name="role"
            value="parent"
            checked={role === "parent"}
            onChange={() => setRole("parent")}
          />
          Parent
        </label>
        <label className={role === "expert" ? "is-selected" : ""}>
          <input
            type="radio"
            name="role"
            value="expert"
            checked={role === "expert"}
            onChange={() => setRole("expert")}
          />
          Medical expert
        </label>
      </fieldset>

      <button className="auth-primary" type="submit" disabled={submitting}>
        {submitting ? "Creating account..." : "Create account"}
      </button>

      {message && <p className="auth-message">{message}</p>}

      <p className="auth-switch">
        Already have an account?{" "}
        <button type="button" onClick={onSwitchToLogin}>
          Log in
        </button>
      </p>
    </form>
  );
}
