type NavIconProps = {
  label: string;
};

export function HomeIcon({ label }: NavIconProps) {
  return (
    <svg aria-label={label} width="22" height="22" viewBox="0 0 24 24" fill="none">
      <path
        d="M4 10.6 12 4l8 6.6V20a1 1 0 0 1-1 1h-5v-6h-4v6H5a1 1 0 0 1-1-1v-9.4Z"
        fill="#697586"
      />
    </svg>
  );
}

export function AccountIcon({ label }: NavIconProps) {
  return (
    <svg aria-label={label} width="22" height="22" viewBox="0 0 24 24" fill="none">
      <path
        d="M12 12a4 4 0 1 0 0-8 4 4 0 0 0 0 8Zm-7 8a7 7 0 0 1 14 0H5Z"
        fill="#697586"
      />
    </svg>
  );
}
