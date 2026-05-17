import type { User } from "firebase/auth";

export type UserRole = "parent" | "expert";

export type UserProfile = {
  uid: string;
  email: string;
  name: string;
  role: UserRole;
  securityCode?: string;
  createdAt?: unknown;
  updatedAt?: unknown;
};

export type AuthContextValue = {
  firebaseUser: User | null;
  profile: UserProfile | null;
  loading: boolean;
  login: (email: string, password: string) => Promise<void>;
  signup: (
    name: string,
    email: string,
    password: string,
    role: UserRole
  ) => Promise<void>;
  logout: () => Promise<void>;
};
