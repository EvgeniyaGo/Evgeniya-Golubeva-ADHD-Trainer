import { useState, type FormEvent } from "react";

type SignupFormProps = {
  onSwitchToLogin: () => void;
};

export function SignupForm({ onSwitchToLogin }: SignupFormProps) {
  const [role, setRole] = useState("Parent");
  const [message, setMessage] = useState("");

  const handleSubmit = (event: FormEvent<HTMLFormElement>) => {
    event.preventDefault();
    setMessage("Account creation will be connected later.");
    console.log("Signup placeholder submitted", { role });
  };

  return (
    <form className="auth-form" onSubmit={handleSubmit}>
      <div className="auth-heading">
        <h2 id="auth-modal-title">Create account</h2>
        <p>Prepare a profile for tracking cube training progress.</p>
      </div>

      <label className="auth-field">
        <span>Name</span>
        <input type="text" name="name" autoComplete="name" />
      </label>

      <label className="auth-field">
        <span>Email</span>
        <input type="email" name="email" autoComplete="email" />
      </label>

      <label className="auth-field">
        <span>Password</span>
        <input type="password" name="password" autoComplete="new-password" />
      </label>

      <fieldset className="auth-role">
        <legend>Role</legend>
        <label className={role === "Parent" ? "is-selected" : ""}>
          <input
            type="radio"
            name="role"
            value="Parent"
            checked={role === "Parent"}
            onChange={(event) => setRole(event.target.value)}
          />
          Parent
        </label>
        <label className={role === "Medical expert" ? "is-selected" : ""}>
          <input
            type="radio"
            name="role"
            value="Medical expert"
            checked={role === "Medical expert"}
            onChange={(event) => setRole(event.target.value)}
          />
          Medical expert
        </label>
      </fieldset>

      <button className="auth-primary" type="submit">
        Create account
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
