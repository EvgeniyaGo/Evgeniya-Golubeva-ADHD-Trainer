import { useState, type FormEvent } from "react";

type LoginFormProps = {
  onSwitchToSignup: () => void;
};

export function LoginForm({ onSwitchToSignup }: LoginFormProps) {
  const [message, setMessage] = useState("");

  const handleSubmit = (event: FormEvent<HTMLFormElement>) => {
    event.preventDefault();
    setMessage("Login will be connected later.");
    console.log("Login placeholder submitted");
  };

  return (
    <form className="auth-form" onSubmit={handleSubmit}>
      <div className="auth-heading">
        <h2 id="auth-modal-title">Log in</h2>
        <p>Access your cube profile and saved training sessions.</p>
      </div>

      <label className="auth-field">
        <span>Email</span>
        <input type="email" name="email" autoComplete="email" />
      </label>

      <label className="auth-field">
        <span>Password</span>
        <input type="password" name="password" autoComplete="current-password" />
      </label>

      <button className="auth-primary" type="submit">
        Log in
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
